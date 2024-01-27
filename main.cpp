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
      hrirL[i][j++] = Parse<float>(s_line);

    reader.open(U"hrtfs/elev{}/R{}e{:0>3}a.dat"_fmt(ELEV, ELEV, i * 5));
    if (!reader)
      throw Error{U"Failed to open hrtf file"};
    j = 0;
    while (reader.readLine(s_line))
      hrirR[i][j++] = Parse<float>(s_line);
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

Wave applyHRTF(const Array<float> &samples, const Array<Array<Array<float>>> &HRTF, int32 theta) {
  Wave wave{samples.size() + 2 * MEASUREMENT_PINTS};
  fftsg::RFFTEngine<float> rfft(MEASUREMENT_PINTS * 2);
  theta %= 360;

  for (size_t i = 0; i < samples.size() / CHG_LEN + 1; i++) {
    float m = (float)theta / 5;
    int m1 = m, m2 = m + 1;
    m2 %= 72;
    float r2 = m - (int)m;
    float r1 = 1.f - r2;

    Array<float> audio_N(samples.begin() + i * CHG_LEN, samples.end() + i * CHG_LEN + MEASUREMENT_PINTS), buff(MEASUREMENT_PINTS, 0);
    audio_N.insert(audio_N.end(), buff.begin(), buff.end());
    rfft.rfft(audio_N.data());

    auto Y1 = HRTF[0][m1].map([r1](float h) { return r1 * h; });
    auto Y2 = HRTF[0][m2].map([r2](float h) { return r2 * h; });
    for (size_t j = 0; j < Y1.size(); j++)
      Y1[j] = (Y1[j] + Y2[j]) * audio_N[j];

    rfft.irfft(Y1.data());
    for (size_t j = 0; j < 2 * MEASUREMENT_PINTS; j++)
      wave[i * CHG_LEN + j].left += Y1[j];

    Y1 = HRTF[1][m1].map([r1](float h) { return r1 * h; });
    Y2 = HRTF[1][m2].map([r2](float h) { return r2 * h; });
    for (size_t j = 0; j < Y1.size(); j++)
      Y1[j] = (Y1[j] + Y2[j]) * audio_N[j];

    rfft.irfft(Y1.data());
    for (size_t j = 0; j < 2 * MEASUREMENT_PINTS; j++)
      wave[i * CHG_LEN + j].right += Y1[j];
  }

  return wave;
}

auto HRTF = getHRTF();
class MyAudioStream : public IAudioStream {
 public:
  void setFrequency(int32 frequency) {
    m_oldFrequency = m_frequency.load();

    m_frequency.store(frequency);
  }

  void setTheta(int32 theta) {
    m_theta.store(theta);
  }

 private:
  size_t m_pos = 0;

  std::atomic<int32> m_oldFrequency = 440;

  std::atomic<int32> m_frequency = 440;

  std::atomic<int32> m_theta = 0;

  void getAudio(float *left, float *right, const size_t samplesToWrite) override {
    const int32 oldFrequency = m_oldFrequency;
    const int32 frequency = m_frequency;
    const float blend = (1.0f / samplesToWrite);

    Array<float> samples;
    for (size_t i = 0; i < samplesToWrite; i++) {
      const float t0 = (2_piF * oldFrequency * (static_cast<float>(m_pos) / Wave::DefaultSampleRate));
      const float t1 = (2_piF * frequency * (static_cast<float>(m_pos) / Wave::DefaultSampleRate));
      const float a = (Math::Lerp(std::sin(t0), std::sin(t1), (blend * i))) * 0.5f;

      samples.emplace_back(a);

      m_pos++;
    }

    auto wave = applyHRTF(samples, HRTF, m_theta);
    for (size_t i = 0; i < samplesToWrite; i++) {
      *left++ = wave[i].left;
      *right++ = wave[i].right;
    }

    m_oldFrequency = frequency;

    m_pos %= Math::LCM(frequency, Wave::DefaultSampleRate);
  }

  bool hasEnded() override {
    return false;
  }

  void rewind() override {
    m_pos = 0;
  }
};

void Main(void) {
  // const Audio audio{U"output.wav"};
  // Wave s;
  // auto left = audio.samples();
  // for (int i = 0; i <= 36; i++) {
  //   auto size = audio.samples() / 37;
  //   Array<float> samples{audio.getSamples(0) + size * i, audio.getSamples(0) + size * (i + 1)};
  //   s.append(applyHRTF(samples, HRTF, i * 10));
  // }
  // const Audio sound{s};
  // sound.play();
  std::shared_ptr<MyAudioStream> audioStream = std::make_shared<MyAudioStream>();

  Audio ss{audioStream};
  ss.setVolume(0.1);
  ss.play();
  double frequency = 440.0, theta = 0.0;

  while (System::Update()) {
    if (SimpleGUI::Slider(U"{}Hz"_fmt(static_cast<int32>(frequency)), frequency, 220.0, 880.0, Vec2{40, 40}, 120, 200)) {
      audioStream->setFrequency(static_cast<int32>(frequency));
    }
    if (SimpleGUI::Slider(U"theta:{}"_fmt(static_cast<int32>(theta)), theta, 0.0, 360.0, Vec2{40, 80}, 120, 200)) {
      audioStream->setTheta(static_cast<int32>(theta));
    }
  }
}
