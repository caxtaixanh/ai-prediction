# predictive-input-buffer (Android)

Trajectory-based 1-2 frame press and wave release input buffer.

Ported from the original Windows `.geode` by lilWC.

---

## How to use (Android)

Open the **pause menu** mid-level and tap the **[ Buffer S1 – … ]** button at the bottom to cycle through stages. A brief label appears inside the level confirming the new stage.

| Stage | All modes | Wave mode | Approx. frames @ 60 fps |
|-------|-----------|-----------|--------------------------|
| 1     | 2 ticks   | 2 ticks   | 0.5 f                    |
| 2     | 4 ticks   | 4 ticks   | 1.0 f                    |
| 3     | 8 ticks   | 8 ticks   | 2.0 f                    |
| 4     | 16 ticks  | 8 ticks   | 4.0 f / 2.0 f            |
| 5     | OFF       | OFF       | raw input, no buffer     |

---

## Build instructions

### Prerequisites

- [Geode SDK](https://geode-sdk.org) cloned and `GEODE_SDK` env var set  
- Android NDK r25c or later  
- CMake 3.21+  
- A Geode cross-compilation toolchain (`geode sdk install-binaries` handles this)

### Configure & build (arm64-v8a)

```bash
mkdir build && cd build

cmake .. \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-23 \
  -DANDROID_STL=c++_static

cmake --build . --parallel
```

The built `.geode` package will appear in `build/ai.predictivebuffer.geode`.

### armeabi-v7a (32-bit, older devices)

Replace `-DANDROID_ABI=arm64-v8a` with `-DANDROID_ABI=armeabi-v7a`. Both `.geode` files can coexist — `mod.json` declares both `android32` and `android64` targets.

---

## Differences from the Windows version

| Feature | Windows | Android |
|---------|---------|---------|
| Stage cycle key | F6 in-level | Pause menu button |
| Stage HUD | In-level label | Same |
| End-level stats | `EndLevelLayer` label | Same |
| All 17 hooks | ✓ | ✓ |
| Offsets | Win 2.2081 | Resolved by Geode at load time |
