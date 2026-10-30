#pragma once

// Volume trims (dB) that bring every preset to about the same loudness. Regenerate with: SkacidTest --trim Source/PresetTrims.h
// (run it 3 times: each pass starts from the previous trims)
namespace skacid { inline constexpr float presetTrims[] = {
    3.71f,    3.03f,    2.57f,    -0.19f,    3.91f,    -3.99f,    1.82f,    1.21f,
    4.89f,    -4.50f,    -3.18f,    -3.39f,    -3.09f,    3.34f,    -0.31f,    0.63f,
    5.46f,    6.28f,    9.30f,    5.35f,    6.59f,    6.20f,    10.80f,    0.39f,
    2.19f,    -5.09f,    -3.36f,    -1.99f,    -3.70f,    5.04f,    -2.31f,    -3.79f,
    -1.07f,    -2.66f,    3.06f,    0.55f,    6.00f,    5.16f,    3.82f,    2.36f,
    0.69f,    -6.04f,    -6.59f,    -2.89f,    1.14f,    -5.66f,    1.72f,    -5.38f,
    -4.79f,    -4.76f,    0.74f,    -5.69f,    -0.52f,    -1.07f,    -4.52f,    -6.61f,
    1.01f,    -5.80f,    -7.45f,    -4.80f,    -4.34f,    3.76f,    -5.56f,    0.68f,
    0.71f,    -5.66f,    -3.30f,    2.73f,    -0.45f,    -1.17f,    -5.44f,    -2.51f,
    -3.27f,    10.01f,    9.50f,    4.75f,    10.20f,    6.00f,    -0.02f,    2.96f,
    3.60f,    1.90f,    -7.51f,    -4.55f,    -3.63f,    -4.76f,    -6.18f,    -1.21f,
    -0.93f,    -9.63f,    -4.94f,    -5.35f,    -10.30f,    6.00f,    1.11f,    1.45f,
    4.04f,    -3.87f,    -2.95f,    -0.07f,    -6.95f,    6.00f,    -0.58f,    -2.21f,
    6.00f,    10.70f,    1.91f,    3.83f,    9.80f,    6.00f,    0.51f,    1.77f,
    3.17f,    -1.42f,    -2.56f,    -3.26f,    -1.86f,    -0.77f,    -3.13f,    -0.12f,
    -7.11f,    -5.02f,    -7.16f,    -2.52f,    -9.02f,    0.11f,    3.79f,    -0.56f,
    6.00f,
}; }
