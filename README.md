# RocketForge submission

GPU inference and team batching are enabled by default. This package contains
only the selected bot's inference sources and checkpoint, without a dependency
on the RocketForge installation.

## Build

Run `python scripts/build_bob.py` with Bob and Docker installed. The helper uses
the selected Windows SDK from the shared RLBot bot pack. To use a different SDK:

```
python scripts/build_bob.py --libtorch "C:/path/to/libtorch"
```

`LIBTORCH_CUDA_ROOT` (GPU) and `LIBTORCH_CPU_ROOT` (CPU) also select the SDK for
building and running. Use the same LibTorch version as the host. Only headers
and import libraries enter the container; runtime DLLs stay in the shared pack.
The helper fingerprints the SDK to invalidate Bob's cache when it changes.

## Device

`rlbot/device.txt` selects `gpu` (default) or `cpu` **before building**. Rebuild
after changing it. GPU packages support Windows with a compatible NVIDIA GPU
and driver. CPU packages support Windows and Linux. GPU builds never silently
fall back to CPU. Startup validates CUDA and warms up the actual policy before
RLBot starts sending live packets.

On Windows the GPU launcher looks for `libtorch/lib` in nearby bot-pack folders,
then `%LOCALAPPDATA%/RLBot5/bots/libtorch/lib`. CPU uses `torch-archive/torch/lib`.
An explicit device-specific environment override takes precedence and must be
valid. The launcher loads only the matching `000-runtime/RLBotCoreCUDA.exe` or
`RLBotCoreCPU.exe`. It does not download or bundle shared runtime DLLs.

For a native Windows build, use Release or RelWithDebInfo and point CMake at the
selected SDK with `-DLIBTORCH_ROOT=...`. The launcher belongs beside `bot.toml`;
the matching core, models, agent_setup.json, prediction libraries and collision
meshes belong in `000-runtime` (Bob arranges this automatically).

Team batching is independent of device selection. Keep `hivemind` and the
`--hivemind` argument in `rlbot/bot.toml` consistent when editing them manually.

Build and launcher behavior follows the public
[GGLBot bob-gpu branch](https://github.com/SubparN0va/GGLBot/tree/bob-gpu).


## Optional recording and replay

Append `--capture-dir "E:/rlbot-captures/match"` to the bot's run command to record
callback packets, observations, decisions and returned controls. Recording is off
by default. Each worker writes a unique `callbacks.rfcapture`; stop normally to
flush its footer. Keep these diagnostic files outside the submission package.

Replay without connecting to the game (use your package's matching device):

```
RLBot.exe --replay "E:/rlbot-captures/match/capture-ID/callbacks.rfcapture" --model-dir "000-runtime" --capture-dir "E:/rlbot-captures/replay" --device gpu
python scripts/rlbot_capture.py compare "E:/rlbot-captures/match/capture-ID/callbacks.rfcapture" "E:/rlbot-captures/replay" --html "E:/rlbot-captures/report.html"
```

`--hivemind off` replays team-recorded cars separately. `--allow-incomplete`
explicitly permits a verified interrupted prefix; corrupt or missing callbacks
are rejected. Recording adds diagnostic overhead and should be disabled for
normal matches. Replay checks decisions against recorded inputs, not the physics
outcome of changed controls.
