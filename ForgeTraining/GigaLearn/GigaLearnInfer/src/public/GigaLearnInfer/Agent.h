#pragma once

#include "ModelConfig.h"

#include <filesystem>
#include <string>

namespace c10
{
	struct Device;
}

namespace GGL
{
	class ModelSet;
}

namespace RLGC
{
	class ObsBuilder;
	class ActionParser;
}

class RG_IMEXPORT Agent
{
public:
	Agent();

	virtual ~Agent() = default;

	/** Creates the Observation Builder used to define the inputs for the network */
	virtual RLGC::ObsBuilder* MakeObsBuilder() const;

	/** Wraps even custom observation builders with this agent's saved prediction settings. **/
	RLGC::ObsBuilder* MakeConfiguredObsBuilder() const;

	/** Simulator used for new training builds. Saved checkpoints retain their original simulator. **/
	int trainingEngineVersion = 3;

	/** Ball predictor: 0 follows trainingEngineVersion; 2 and 3 explicitly select RocketSim. **/
	int ballPredictionEngineVersion = 0;

	/** Resolves and validates the prediction engine independently of the match simulator. **/
	int GetBallPredictionEngineVersion() const;

	/** Returns the number of inputs the Obs uses */
	virtual int GetObsSize() const;

	/** Returns the largest symmetric team size eligible for model benchmarking. **/
	virtual int GetMaxBenchmarkTeamSize() const;

	/** Creates the Action Parser used to define the outputs for the network */
	virtual RLGC::ActionParser* MakeActionParser() const;

	/** Clones this Agent while preserving derived type information */
	virtual std::unique_ptr<Agent> Clone() const;

	/** Constructs the policy models used by inference-only runtimes. **/
	virtual void MakeInferenceModels(
		int obsSize,
		int numActions,
		const c10::Device& device,
		GGL::ModelSet& outModels) const;

	/** Name of agent. Used for training selection, checkpoint path, and wandb project name */
	std::string agentName;

	/** Named observation builder selected by saved Agent setup metadata. */
	std::string obsBuilderName;

	/** Named action parser selected by saved Agent setup metadata. */
	std::string actionParserName;

	/** Path to the folder containing POLICY.lt and SHARED_HEAD.lt */
	std::filesystem::path modelPath;

	/** True when the packaged policy can be evaluated but cannot be resumed for training. **/
	bool inferenceOnly = false;

	/** Whether frozen-opponent inference should always choose the highest-logit action. **/
	bool inferenceDeterministic = false;

	/** Game mode this agent plays */
	RocketSim::GameMode gameMode;

	/** RocketSim hitbox configuration this agent uses in simulation */
	RocketSim::CarConfig carConfig = RocketSim::CAR_CONFIG_OCTANE;

	/** How many ticks should be skipped between each inference when training agent. Higher = faster training, lower = faster reaction time and input speed */
	int tickSkip = 8;

	/** 
	* How many ticks should there be between the observing the state and inferring an action
	* Example: With a tickSkip of 8 and actionDelay of 7: the environment will step 7 ticks with old actions, 
	*	new actions will be inferred based on the previous observation, the actions will be applied for the remaining 1 tick,
	*	than rewards are given based on the actions and a new observation is built.
	*/
	int actionDelay = tickSkip - 1;

	/** Maximum team size used to construct Obs */
	int maxTeamSize = 1;

	/** Whether runtime inference observations need GameState::GetFutureBallState(). **/
	bool useBallPrediction = false;

	/** Used to set addLayerNorm of all networks */
	bool addLayerNorm = true;

	/** Used to set optimType of all networks */
	GGL::ModelOptimType otimType = GGL::ModelOptimType::ADAM;

	/** Used to set activationType of all networks */
	GGL::ModelActivationType modelActivationType = GGL::ModelActivationType::RELU;

	/** Partial configs needed by infer for policy, critic, and shared head */
	GGL::PartialModelConfig sharedHead, policy, critic;

	/**
	 * Train the critic with one value head per reward instead of one combined value.
	 * It changes the critic's checkpoint format, so it can't be toggled on an existing build.
	 */
	bool decomposedCritic = false;
};