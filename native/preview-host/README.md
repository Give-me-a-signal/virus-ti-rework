# Actual native VST3 interface host

`build/VirusTIInterface.app` opens the rebuilt plug-in's real Cocoa editor in a native window. It dynamically loads a stable copy of the arm64 `.vst3` stored inside the app's `Contents/Resources`, so rebuilding the workspace plug-in does not replace code underneath an open preview.

This is a small editor host built against the existing pinned Steinberg SDK. `PlugProvider` creates and initializes the actual component and controller and connects their messaging interfaces. The host attaches the actual `IPlugView` to an `NSView`, supplies `IPlugFrame` and `IComponentHandler`, and propagates mapped parameter edits into the real inactive processor with zero-sample parameter flushes. It never activates the processor, creates an audio device/callback, starts streaming, opens USB/MIDI, or loads vendor code.

The plug-in itself supplies the disconnected indication. This app is for trying the interface, not listening to the instrument. Values live for the current preview session; the host does not save a project or patch. Close the window or press Command-Q to exit.

Build and headless verification:

```sh
./native/preview-host/build_and_test.sh
```

The self-test dynamically loads the embedded `.vst3`, initializes its component/controller, attaches its real `IPlugView` to an undisplayed native view, edits a parameter through the host handler, verifies that edit by round-tripping the processor state back into the controller, then detaches and terminates. No window is displayed by this test. It passed on this Apple-silicon Mac. Both app and plug-in are locally ad-hoc signed; nothing is installed in system or user plug-in directories.

The preview snapshot is refreshed at host link time. To replace it after changing the plug-in, first quit the preview app, complete the plug-in build/tests, then run:

```sh
./native/preview-host/refresh_plugin.sh
```

The refresh script refuses while the host process is running, verifies the copied plug-in signature, reruns the actual attachment/edit test and signs the app. Open the app again after success. The application can also accept an explicit `.vst3` path as its first command-line argument, but the normal Finder/CUA launch always uses its embedded snapshot.

Primary implementation references: pinned SDK `public.sdk/samples/vst-hosting/editorhost`, `hosting/module`, `hosting/plugprovider`, and `hosting/hostclasses`. This host contains no copy of the editor implementation: its window displays the editor supplied by the loaded plug-in.

## Modern Dark / Polar candidate

`build-modern/VirusTIModernPreview.app` is a separate offline app embedding the tested `native/vst3/build-modern` candidate. Its window forwards host resize requests and enforces the editor aspect ratio. The footer switches Dark/Polar and 100–200% zoom; the initial preference is 125%. On smaller displays, use 100% or 125%. This preview supports editing and browser exploration, but does not activate audio.

CMake options `VTR_PREVIEW_OUTPUT`, `VTR_PREVIEW_ID` and `VTR_PREVIEW_NAME` let this candidate coexist with the earlier preview. Its configured values are `VirusTIModernPreview`, `org.virus-ti-rework.modern-preview` and `Virus TI Modern Preview`; `VTR_PLUGIN_PATH` points at the dedicated modern VST3 bundle. Refresh this candidate by quitting it and relinking its `build-modern` target after rebuilding the plug-in. The default refresh script above targets the original preview only.
