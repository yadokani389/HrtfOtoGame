#include <Siv3D.hpp>

#include "fftsg/rfft_engine.hpp"

constexpr int MEASUREMENT_PINTS = 512;
constexpr int ELEV = 0;
constexpr int CHG_LEN = 128;
constexpr int ANGULAR_VELOCITY = 30;

Array<Array<Array<float>>> getHRTF(void) {
  std::vector hrirL(72, std::vector<float>(MEASUREMENT_PINTS * 2, 0));
  auto hrirR = hrirL;
  for (int i = 0; i < 360 / 5; i++) {
    TextReader reader{U"hrtfs/elev{}/L{}e{:0>3}a.dat"_fmt(ELEV, ELEV, i * 5)};
    if (!reader)
      throw Error{U"Failed to open hrtf file"};
    int j = 0;
    String s_line;
    while (reader.readLine(s_line))
      hrirL[i][j++] = (Parse<float>(s_line));

    reader.open(U"hrtfs/elev{}/R{}e{:0>3}a.dat"_fmt(ELEV, ELEV, i * 5));
    if (!reader)
      throw Error{U"Failed to open hrtf file"};
    j = 0;
    while (reader.readLine(s_line))
      hrirR[i][j++] = (Parse<float>(s_line));
  }

  Array<Array<Array<float>>> HRTF(2, Array<Array<float>>(72, Array<float>(MEASUREMENT_PINTS + 1, 0)));
  fftsg::RFFTEngine<float> rfftEngine(MEASUREMENT_PINTS * 2);
  for (int i = 0; i < 72; i++) {
    rfftEngine.rfft(hrirL[i]);
    rfftEngine.rfft(hrirR[i]);
    HRTF[0][i] = hrirL[i];
    HRTF[1][i] = hrirR[i];
  }

  return HRTF;
}

Wave makeHRTF(const Audio &audio, const Array<Array<Array<float>>> &HRTF) {
  Wave wave{audio.samples()};
  auto samples = audio.getSamples(0);
  fftsg::RFFTEngine<float> rfft(MEASUREMENT_PINTS * 2);

  for (size_t i = 0; i < audio.samples() / CHG_LEN + 1; i++) {
    int theta = ANGULAR_VELOCITY * i * CHG_LEN / audio.sampleRate();
    theta %= 360;

    float m = (float)theta / 5;
    int m1 = m, m2 = m + 1;
    if (m2 == 72)
      m2 = 0;
    float r2 = m - (int)m;
    float r1 = 1.f - r2;

    Array<float> audio_N(samples + i * CHG_LEN, samples + i * CHG_LEN + MEASUREMENT_PINTS), buff(MEASUREMENT_PINTS, 0);
    audio_N.insert(audio_N.end(), buff.begin(), buff.end());
    rfft.rfft(audio_N.data());

    auto Y1 = HRTF[0][m1].map([&](float h) { return r1 * h; });
    auto Y2 = HRTF[0][m2].map([&](float h) { return r2 * h; });
    for (size_t j = 0; j < Y1.size(); j++)
      Y1[j] = (Y1[j] + Y2[j]) * audio_N[j];

    rfft.irfft(Y1.data());
    for (size_t j = 0; j < 2 * MEASUREMENT_PINTS; j++)
      wave[i * CHG_LEN + j].left += Y1[j];

    Y1 = HRTF[1][m1].map([&](float h) { return r1 * h; });
    Y2 = HRTF[1][m2].map([&](float h) { return r2 * h; });
    for (size_t j = 0; j < Y1.size(); j++)
      Y1[j] = (Y1[j] + Y2[j]) * audio_N[j];

    rfft.irfft(Y1.data());
    for (size_t j = 0; j < 2 * MEASUREMENT_PINTS; j++)
      wave[i * CHG_LEN + j].right += Y1[j];
  }

  return wave;
}

void Main(void) {
  auto HRTF = getHRTF();
  const Audio audio{U"output.wav"};
  const Audio sound{makeHRTF(audio, HRTF)};
  sound.play();
  while (System::Update()) {
  }
}
