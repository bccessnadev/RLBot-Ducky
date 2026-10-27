#pragma once

#include <RLGymCPP/ObsBuilders/ObsBuilder.h>

namespace RLGC
{
	/**
	 * Advance padded obs with prediction and local space tracking for other cars.
	 * Doesn't override any AdvancedObs because custom AddPlayerToObs is needed.
	 */
	class DuckyObs : public ObsBuilder {
	public:

		constexpr static float
			POS_COEF = 1 / 5000.f,
			VEL_COEF = 1 / 2300.f,
			ANG_VEL_COEF = 1 / 3.f;

		int maxPlayers = 3;

		DuckyObs(const int maxPlayers = 3, const std::vector<float>& ballPredTimes = { 0.3f, 1.f, 3.f })
			: maxPlayers(maxPlayers), ballPredTimes(ballPredTimes) {
			_perPlayerSize = PerPlayerSize();
			_totalSize = TotalSize();
		}

		void Reset(const GameState& initialState) override;

		FList BuildObs(const Player& player, const GameState& state) override;
		void BuildObsInto(const Player& player, const GameState& state, FList& out) override;

		/** Appends the self slot for owningPlayer. NOTE: BuildObsInto only calls this with player == owningPlayer. */
		virtual void AddPlayerToObs(FList& obs, const Player& player, bool inv, const PhysState& ball, const Player& owningPlayer) const;

	protected:

		/** A ball is predicted into the future for each time in this list. */
		std::vector<float> ballPredTimes;

		/**
		 * When true, each ball prediction also emits the predicted ball's angular velocity
		 * (3 floats, after vel and before the local-space pos/vel). Off by default so the
		 * base layout is unchanged; the V3 (GigaLearn) layout sets it on. See DuckyObs_GGL.
		 */
		bool predIncludesAngVel = false;

		/** Floats per single player slot — computed once at construction. */
		int _perPlayerSize = 0;

		/** Total obs size for reserve() — computed once at construction. */
		int _totalSize = 0;

		/**
		 * Writes one player's slot starting at dst, returning the number of floats written.
		 * Must produce exactly _perPlayerSize floats. Layout is identical to AddPlayerToObs.
		 */
		virtual size_t WritePlayerSlot(float* dst, const Player& player, bool inv, const PhysState& ball, const Player& owningPlayer) const;

		/** Returns the constant per-player slot size, independent of game state. */
		virtual int PerPlayerSize() const;

		/** Returns the constant total obs size given maxPlayers and ballPredTimes.size(). */
		virtual int TotalSize() const;
	};
	
	/**
	 * V2 observation layout. Reuses the DuckyObs structure but changes the per-player slots:
	 *  - Drops world-frame angular velocity (the local-frame copy is kept).
	 *  - Drops the always-zero owner-relative position/velocity from the self slot.
	 *  - Adds demo-respawn blend, dodge-window remaining, and isSupersonic to every slot.
	 * Also appends the best reachable big and small boost pad locations (x, y each).
	 */
	class DuckyObs_V2_01 : public DuckyObs {
	public:

		/** Inactive pads respawning within this many seconds still count as boost candidates. */
		constexpr static float PAD_AVAILABLE_SOON_TIME = 3.f;

		DuckyObs_V2_01(const int maxPlayers = 3, const std::vector<float>& ballPredTimes = { 0.3f, 1.f, 3.f })
			: DuckyObs(maxPlayers, ballPredTimes) {
			// The base constructor cached sizes through DuckyObs's virtuals; recompute with ours.
			_perPlayerSize = PerPlayerSize();
			_totalSize = TotalSize();
		}

		void BuildObsInto(const Player& player, const GameState& state, FList& out) override;

		/** Writes the V2 self slot. Owner-relative features are omitted because they are always zero for self. */
		void AddPlayerToObs(FList& obs, const Player& player, bool inv, const PhysState& ball, const Player& owningPlayer) const override;

	protected:

		size_t WritePlayerSlot(float* dst, const Player& player, bool inv, const PhysState& ball, const Player& owningPlayer) const override;

		int PerPlayerSize() const override;

		int TotalSize() const override;

		/** Returns the quickest-to-reach pad location of the given size class */
		Vec GetBestBoostLocation(const Player& player, const GameState& state, const bool bigBoost) const;
	};

	/**
	 * Pre-RocketForge (GigaLearn) V3 observation layout. Identical to DuckyObs except each ball
	 * prediction also includes the predicted ball's angular velocity, and the default prediction
	 * times are V3's ({0.5, 1, 3}s). Needed to load imported V3 builds byte-compatibly.
	 */
	class DuckyObs_GGL : public DuckyObs {
	public:

		DuckyObs_GGL(const int maxPlayers = 3, const std::vector<float>& ballPredTimes = { 0.5f, 1.f, 3.f })
			: DuckyObs(maxPlayers, ballPredTimes) {
			predIncludesAngVel = true;

			// The base constructor cached sizes with predIncludesAngVel off; recompute with it on.
			_perPlayerSize = PerPlayerSize();
			_totalSize = TotalSize();
		}
	};
}
