# Offline audio pipeline integration check

Run `rtk proxy sh native/audio-pipeline-check/build_and_test.sh` from the project root (omit `rtk proxy` if unavailable). This builds an arm64 executable with AddressSanitizer and UndefinedBehaviorSanitizer.

The fixture connects the configuration-descriptor parser, endpoint-capacity validation, nominal packet scheduler, PCM packing/unpacking and SPSC frame queue. It converts one synthetic second of interleaved audio from USB-sized packets into 128-frame host blocks, verifying each channel/sample and exact frame totals.

The 16 cases cover 44.1/48 kHz, full/high-speed interval assumptions, two/six channels and 16/24-bit formats. Invented descriptors and signals are explicitly test data, not captured Virus descriptors or proof of supported hardware modes. No USB framework is linked and no audio device is opened. Device clock feedback, real transfers, latency and DAW integration remain untested.
