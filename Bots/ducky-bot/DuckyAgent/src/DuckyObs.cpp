#include "DuckyObs.h"
#include <RLGymCPP/Gamestates/StateUtil.h>
#include <GigaLearnInfer/Agent.h>
#include <GigaLearnInfer/AgentComponentRegistry.h>

#include <algorithm>
#include <limits>
#include <numeric>

using namespace RocketSim;
using namespace std;

namespace RLGC
{
	// Expresses a world-space point in the local frame defined by rot and origin
	static Vec ToLocalSpace(const RotMat& rot, const Vec& point, const Vec& origin) {
		return rot.Dot(point - origin);
	}
	
	REGISTER_OBS_BUILDER("DuckyObs", [](const Agent& agent) {
	return std::make_unique<RLGC::DuckyObs>(agent.maxTeamSize);
	})

	// Pre-RocketForge (GigaLearn) V3 obs layout: like "DuckyObs" but each ball prediction also
	// carries the predicted ball's ang vel, and prediction times are V3's {0.5, 1, 3}s. Select
	// this for models imported from the old GigaLearn project so their obs matches training.
	REGISTER_OBS_BUILDER("DuckyObs_GGL", [](const Agent& agent) {
	return std::make_unique<RLGC::DuckyObs_GGL>(agent.maxTeamSize);
	})

	void DuckyObs::Reset(const GameState& initialState)
	{
		// Request ball state for maximum prediction time to initialize ball prediction
		if (!ballPredTimes.empty())
			initialState.GetFutureBallState(ballPredTimes[ballPredTimes.size() - 1]);
	}

	int DuckyObs::PerPlayerSize() const {
		// pos(3) + fwd(3) + up(3) + vel(3) + angVel(3) + localAngVel(3) = 18
		// localBallPos(3) + localBallVel(3) = 6
		// localPlayerPosFromOwner(3) + localPlayerVelFromOwner(3) = 6
		// boost(1) + isOnGround(1) + hasFlipOrJump(1) + isDemoed(1) + hasJumped(1) = 5
		return 35;
	}

	int DuckyObs::TotalSize() const {
		// ball pos(3) + vel(3) + angVel(3) = 9
		// prevAction(ELEM_AMOUNT) + boost pads(BOOST_LOCATIONS_AMOUNT) + self(_perPlayerSize)
		// other slots: (maxPlayers-1) teammates + maxPlayers opponents = (2*maxPlayers-1) * _perPlayerSize
		// ball predictions: per time, abs pos(3)+vel(3)+local pos(3)+local vel(3) = 12,
		//   plus abs angVel(3) when predIncludesAngVel (V3/GigaLearn layout) = 15
		const int prevActionLen = Action::ELEM_AMOUNT;
		const int padLen = CommonValues::BOOST_LOCATIONS_AMOUNT;
		const int selfLen = _perPlayerSize;
		const int otherLen = (2 * maxPlayers - 1) * _perPlayerSize;
		const int predLen = (int)ballPredTimes.size() * (predIncludesAngVel ? 15 : 12);
		return 9 + prevActionLen + padLen + selfLen + otherLen + predLen;
	}
	
	FList DuckyObs::BuildObs(const Player& player, const GameState& state) {
		FList out;
		BuildObsInto(player, state, out);
		return out;
	}

	void DuckyObs::BuildObsInto(const Player& player, const GameState& state, FList& out) {
		const bool inv = player.team == Team::ORANGE;

		out.clear();
		out.reserve(_totalSize);

		const PhysState ball = InvertPhys(state.ball, inv);
		auto& pads = state.GetBoostPads(inv);
		auto& padTimers = state.GetBoostPadTimers(inv);

		// Current ball state
		out += ball.pos * POS_COEF;
		out += ball.vel * VEL_COEF;
		out += ball.angVel * ANG_VEL_COEF;

		// Previous actions
		for (int i = 0; i < player.prevAction.ELEM_AMOUNT; i++)
			out += player.prevAction[i];

		// Boost pad states
		for (int i = 0; i < state.GetBoostLocationAmount(); i++) {
			if (pads[i]) {
				out += 1.f;
			} else {
				out += 1.f / (1.f + padTimers[i]);
			}
		}

		// Self slot
		AddPlayerToObs(out, player, inv, ball, player);

		// Flat teammate/opponent slot buffers. Each other player's slot is written
		// to a known offset, then slot indices are shuffled before appending to out.
		const int slotSize = _perPlayerSize;
		const int teammatesCount = maxPlayers - 1;
		const int opponentsCount = maxPlayers;

		std::vector<float> teammates(teammatesCount * slotSize, 0.f);
		std::vector<float> opponents(opponentsCount * slotSize, 0.f);

		int teammateIdx = 0;
		int opponentIdx = 0;

		for (const Player& otherPlayer : state.players) {
			if (otherPlayer.carId == player.carId)
				continue;

			float* dst;
			if (otherPlayer.team == player.team) {
				if (teammateIdx >= teammatesCount)
					RG_ERR_CLOSE("DuckyObs: Too many teammates for Obs, maximum is " << teammatesCount);
				dst = teammates.data() + teammateIdx * slotSize;
				teammateIdx++;
			} else {
				if (opponentIdx >= opponentsCount)
					RG_ERR_CLOSE("DuckyObs: Too many opponents for Obs, maximum is " << opponentsCount);
				dst = opponents.data() + opponentIdx * slotSize;
				opponentIdx++;
			}

			[[maybe_unused]] const size_t written = WritePlayerSlot(dst, otherPlayer, inv, ball, player);
			RG_ASSERT(written == (size_t)slotSize);
		}

		// Index-based shuffle preserves the same permutation distribution as the
		// prior std::shuffle over std::vector<FList> — same engine, same draw order.
		std::vector<int> teammateOrder(teammatesCount);
		std::vector<int> opponentOrder(opponentsCount);
		std::iota(teammateOrder.begin(), teammateOrder.end(), 0);
		std::iota(opponentOrder.begin(), opponentOrder.end(), 0);
		std::shuffle(teammateOrder.begin(), teammateOrder.end(), ::Math::GetRandEngine());
		std::shuffle(opponentOrder.begin(), opponentOrder.end(), ::Math::GetRandEngine());

		for (int i : teammateOrder)
			out.insert(out.end(), teammates.begin() + i * slotSize, teammates.begin() + (i + 1) * slotSize);
		for (int i : opponentOrder)
			out.insert(out.end(), opponents.begin() + i * slotSize, opponents.begin() + (i + 1) * slotSize);

		// Ball prediction
		for (size_t i = 0; i < ballPredTimes.size(); i++) {
			const BallState predBallState = state.GetFutureBallState(ballPredTimes[i]);
			const PhysState predBall = InvertPhys(predBallState, inv);
			const PhysState playerPhys = InvertPhys(player, inv);

			out += predBall.pos * POS_COEF;
			out += predBall.vel * VEL_COEF;

			// V3 (GigaLearn) layout emits the predicted ball's world-frame ang vel here.
			if (predIncludesAngVel)
				out += predBall.angVel * ANG_VEL_COEF;

			out += ToLocalSpace(playerPhys.rotMat, predBall.pos, playerPhys.pos) * POS_COEF;
			out += ToLocalSpace(playerPhys.rotMat, predBall.vel, playerPhys.vel) * VEL_COEF;
		}
	}

	void DuckyObs::AddPlayerToObs(FList& obs, const Player& player, bool inv, const PhysState& ball, const Player& owningPlayer) const {
		const PhysState phys = InvertPhys(player, inv);

		obs += phys.pos * POS_COEF;
		obs += phys.rotMat.forward;
		obs += phys.rotMat.up;
		obs += phys.vel * VEL_COEF;
		obs += phys.angVel * ANG_VEL_COEF;
		obs += phys.rotMat.Dot(phys.angVel) * ANG_VEL_COEF;

		obs += ToLocalSpace(phys.rotMat, ball.pos, phys.pos) * POS_COEF;
		obs += ToLocalSpace(phys.rotMat, ball.vel, phys.vel) * VEL_COEF;

		const PhysState owningPhys = InvertPhys(owningPlayer, owningPlayer.team == Team::ORANGE);
		obs += ToLocalSpace(owningPhys.rotMat, phys.pos, owningPhys.pos) * POS_COEF;
		obs += ToLocalSpace(owningPhys.rotMat, phys.vel, owningPhys.vel) * VEL_COEF;

		obs += player.boost / 100;
		obs += player.isOnGround;
		obs += player.HasFlipOrJump();
		obs += player.isDemoed;
		obs += player.hasJumped;
	}

	size_t DuckyObs::WritePlayerSlot(float* dst, const Player& player, bool inv, const PhysState& ball, const Player& owningPlayer) const {
		const PhysState phys = InvertPhys(player, inv);
		float* writePtr = dst;

		auto writeVec = [&](const Vec& v) {
			*writePtr++ = v.x; *writePtr++ = v.y; *writePtr++ = v.z;
		};

		writeVec(phys.pos * POS_COEF);
		writeVec(phys.rotMat.forward);
		writeVec(phys.rotMat.up);
		writeVec(phys.vel * VEL_COEF);
		writeVec(phys.angVel * ANG_VEL_COEF);
		writeVec(phys.rotMat.Dot(phys.angVel) * ANG_VEL_COEF);

		writeVec(ToLocalSpace(phys.rotMat, ball.pos, phys.pos) * POS_COEF);
		writeVec(ToLocalSpace(phys.rotMat, ball.vel, phys.vel) * VEL_COEF);

		const PhysState owningPhys = InvertPhys(owningPlayer, owningPlayer.team == Team::ORANGE);
		writeVec(ToLocalSpace(owningPhys.rotMat, phys.pos, owningPhys.pos) * POS_COEF);
		writeVec(ToLocalSpace(owningPhys.rotMat, phys.vel, owningPhys.vel) * VEL_COEF);

		*writePtr++ = player.boost / 100.f;
		*writePtr++ = (float)player.isOnGround;
		*writePtr++ = (float)player.HasFlipOrJump();
		*writePtr++ = (float)player.isDemoed;
		*writePtr++ = (float)player.hasJumped;

		return writePtr - dst;
	}
	
	// Fraction of the dodge window remaining: 1 when grounded or carrying a flip reset,
	// decaying to 0 as the post-jump window expires or once the flip/double-jump is spent.
	static float GetDodgeWindowRemaining(const Player& player) {
		if (player.isOnGround)
			return 1.f;

		if (player.hasFlipped || player.hasDoubleJumped)
			return 0.f;

		return std::max(0.f, 1.f - player.airTimeSinceJump / RLConst::DOUBLEJUMP_MAX_DELAY);
	}

	// Blended demo respawn timer, matching the boost pad timer blend: 1 when alive or
	// about to respawn, approaching 0.25 for a fresh 3-second demo.
	static float GetDemoRespawnBlend(const Player& player) {
		return 1.f / (1.f + player.demoRespawnTimer);
	}
	
	// NOTE: Not obs-compatible with "DuckyObs" — only select this for builds trained from scratch on it
	REGISTER_OBS_BUILDER("DuckyObs_V2_01", [](const Agent& agent) {
		return std::make_unique<RLGC::DuckyObs_V2_01>(agent.maxTeamSize);
	})

	int DuckyObs_V2_01::PerPlayerSize() const {
		// pos(3) + fwd(3) + up(3) + vel(3) + localAngVel(3) = 15
		// localBallPos(3) + localBallVel(3) = 6
		// localPlayerPosFromOwner(3) + localPlayerVelFromOwner(3) = 6
		// boost(1) + isOnGround(1) + hasFlipOrJump(1) + isDemoed(1) + hasJumped(1) = 5
		// demoRespawnBlend(1) + dodgeWindowRemaining(1) + isSupersonic(1) = 3
		return 35;
	}

	int DuckyObs_V2_01::TotalSize() const {
		// Base layout, minus the owner-relative features omitted from the self slot (6),
		// plus the best big + small boost pad locations (x, y each = 4).
		return DuckyObs::TotalSize() - 6 + 4;
	}

	void DuckyObs_V2_01::BuildObsInto(const Player& player, const GameState& state, FList& out) {
		DuckyObs::BuildObsInto(player, state, out);

		const bool inv = player.team == Team::ORANGE;
		const PhysState phys = InvertPhys(player, inv);

		// Best boost pad locations in the player's local frame (forward/right offset, same
		// convention as local ball pos). GetBestBoostLocation returns the pad in the inverted
		// frame; local space is invariant to the inversion, so localizing here is exact.
		auto addLocalXYLocation = [&](const Vec& padLocation) {
			const Vec local = ToLocalSpace(phys.rotMat, padLocation, phys.pos);
			out += local.x * POS_COEF;
			out += local.y * POS_COEF;
		};

		constexpr bool BIG_PADS = true;
		addLocalXYLocation(GetBestBoostLocation(player, state, BIG_PADS));
		addLocalXYLocation(GetBestBoostLocation(player, state, !BIG_PADS));
	}

	void DuckyObs_V2_01::AddPlayerToObs(FList& obs, const Player& player, bool inv, const PhysState& ball, const Player& owningPlayer) const {
		// Only used for the self slot, so the owner-relative features (always zero) are omitted
		RG_ASSERT(player.carId == owningPlayer.carId);

		const PhysState phys = InvertPhys(player, inv);

		obs += phys.pos * POS_COEF;
		obs += phys.rotMat.forward;
		obs += phys.rotMat.up;
		obs += phys.vel * VEL_COEF;
		obs += phys.rotMat.Dot(phys.angVel) * ANG_VEL_COEF;

		obs += ToLocalSpace(phys.rotMat, ball.pos, phys.pos) * POS_COEF;
		obs += ToLocalSpace(phys.rotMat, ball.vel, phys.vel) * VEL_COEF;

		obs += player.boost / 100;
		obs += player.isOnGround;
		obs += player.HasFlipOrJump();
		obs += player.isDemoed;
		obs += player.hasJumped;
		obs += GetDemoRespawnBlend(player);
		obs += GetDodgeWindowRemaining(player);
		obs += player.isSupersonic;
	}

	size_t DuckyObs_V2_01::WritePlayerSlot(float* dst, const Player& player, bool inv, const PhysState& ball, const Player& owningPlayer) const {
		const PhysState phys = InvertPhys(player, inv);
		float* writePtr = dst;

		auto writeVec = [&](const Vec& v) {
			*writePtr++ = v.x; *writePtr++ = v.y; *writePtr++ = v.z;
		};

		writeVec(phys.pos * POS_COEF);
		writeVec(phys.rotMat.forward);
		writeVec(phys.rotMat.up);
		writeVec(phys.vel * VEL_COEF);
		writeVec(phys.rotMat.Dot(phys.angVel) * ANG_VEL_COEF);

		writeVec(ToLocalSpace(phys.rotMat, ball.pos, phys.pos) * POS_COEF);
		writeVec(ToLocalSpace(phys.rotMat, ball.vel, phys.vel) * VEL_COEF);

		const PhysState owningPhys = InvertPhys(owningPlayer, owningPlayer.team == Team::ORANGE);
		writeVec(ToLocalSpace(owningPhys.rotMat, phys.pos, owningPhys.pos) * POS_COEF);
		writeVec(ToLocalSpace(owningPhys.rotMat, phys.vel, owningPhys.vel) * VEL_COEF);

		*writePtr++ = player.boost / 100.f;
		*writePtr++ = (float)player.isOnGround;
		*writePtr++ = (float)player.HasFlipOrJump();
		*writePtr++ = (float)player.isDemoed;
		*writePtr++ = (float)player.hasJumped;
		*writePtr++ = GetDemoRespawnBlend(player);
		*writePtr++ = GetDodgeWindowRemaining(player);
		*writePtr++ = (float)player.isSupersonic;

		return writePtr - dst;
	}

	Vec DuckyObs_V2_01::GetBestBoostLocation(const Player& player, const GameState& state, const bool bigBoost) const {
		const bool inv = player.team == Team::ORANGE;
		const PhysState phys = InvertPhys(player, inv);

		// Pad availability arrays are index-aligned with the canonical location list in the inverted frame
		auto& pads = state.GetBoostPads(inv);
		auto& padTimers = state.GetBoostPadTimers(inv);
		const Vec* padLocations = state.GetBoostLocations();
		const int padCount = state.GetBoostLocationAmount();

		// The direction penalty scales with speed: a stopped car scores pads purely by distance,
		// while a supersonic car pays up to 3x distance for a pad directly behind it.
		Vec flatVel = phys.vel;
		flatVel.z = 0;
		const float speed = flatVel.Length();
		const Vec velDir = speed > 1.f ? flatVel / speed : Vec();
		const float speedFrac = std::min(speed / CommonValues::CAR_MAX_SPEED, 1.f);

		Vec bestLocation = {};
		float bestCost = std::numeric_limits<float>::max();

		// Closest pad of the class regardless of availability, as a fallback
		Vec closestLocation = {};
		float closestDist = std::numeric_limits<float>::max();

		for (int i = 0; i < padCount; i++) {
			const Vec& padLocation = padLocations[i];

			// Big pads sit at z = 73, small pads at z = 70
			const bool isBigPad = padLocation.z > 71.f;
			if (isBigPad != bigBoost)
				continue;

			Vec toPad = padLocation - phys.pos;
			toPad.z = 0;
			const float dist = toPad.Length();

			if (dist < closestDist) {
				closestDist = dist;
				closestLocation = padLocation;
			}

			const bool availableInTime = pads[i] || padTimers[i] <= PAD_AVAILABLE_SOON_TIME;
			if (!availableInTime)
				continue;

			// alignment ∈ [-1, 1]: how directly the car's travel points at the pad
			const float alignment = dist > 1.f ? velDir.Dot(toPad / dist) : 1.f;
			const float cost = dist * (1.f + speedFrac * (1.f - alignment));

			if (cost < bestCost) {
				bestCost = cost;
				bestLocation = padLocation;
			}
		}

		// Every pad of this class is on a long cooldown — settle for the closest one
		if (bestCost == std::numeric_limits<float>::max())
			return closestLocation;

		return bestLocation;
	}
}
