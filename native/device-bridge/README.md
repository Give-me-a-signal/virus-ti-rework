# Native device bridge

`include/virus/device_bridge.hpp` is the new host-side connection contract between a serialized device service and one VST3 processor. It is not the legacy shared-memory ABI.

The service owns capture publication and MIDI consumption; the processor owns audio consumption, MIDI publication and lifecycle. Lock-free SPSC queues hold 4,096 six-channel PCM frames and 2,048 copied three-byte MIDI messages. Callback code allocates nothing. Bridge/service must outlive their attached processor; attachment is allowed only while inactive and one processor exclusively claims a bridge.

The real audio engine's `AudioFrameQueue` can be drained by `serviceTransferCapture`. The caller must supply an independently established **plugin sample timeline** mapping, ensure the sample rates agree, discard old engine captures on restart and perform drift correction elsewhere. This adapter does not interpret USB completion timestamps as sample timestamps, resample audio, open a USB device, or infer private tick units. Engine captures currently lack per-frame timestamp metadata, so production synchronization is still unfinished.

MIDI contains generation, block start, exact sample offset, explicit clock domain and copied bytes. Messages become eligible after `completeBlock`; the service must sort by block/sample offset before scheduling, since parameter queues precede input events in storage. The bridge never claims USB delivery precision. Note/CC conversion is ordinary MIDI 1.0; routing catalog part N to MIDI channel N-1 is a new project policy, not a recovered hardware guarantee.

Activation, deactivation, transport seek and disconnect invalidate generations. Old audio and MIDI are discarded rather than replayed; a disconnected generation cannot become ready again. Queued future audio waits; missing frames become zero at the VST3 boundary and increment underrun counters. Queue-full publication explicitly fails. No fake online service is built into the plug-in.

Processor integration tests in `native/vst3/tests.cpp` exercise nonzero PCM across three stereo float/double buses, exact CC/note offsets and bytes, ownership, partial underrun, future frame retention, seek/disconnect/stale rejection, overflow and zero C++ allocations inside exercised process callbacks. Release, ASan/UBSan and official VST3 validator pass. Hardware remains untested.
