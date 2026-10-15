SSTV RX+TX port for x6100_gui 1.0.2 custom baseline

Contains:
- Full Beta 8 SSTV RX/TX implementation
- 1.0.2 DSP audio subscription using dsp_audio_subscribe_float()
- RX sample rate changed from 11025 Hz to 12000 Hz
- TX RF PCM path remains at AUDIO_PLAY_RATE (48 kHz), as in the working 1.0.1 port
- SSTV app action/button integration
- SSTV default pictures/text resources

Install over ~/x6100/x6100_gui.

IMPORTANT: the rootfs assets must also reach the Buildroot target before generating sdcard.img:
  cp -a ~/x6100/x6100_gui/rootfs/. ~/x6100/AetherX6100Buildroot/build/target/
