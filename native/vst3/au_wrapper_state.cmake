# The pinned Steinberg AU wrapper implements only IComponentHandler. Forward
# non-parameter dirty notifications so AU hosts invalidate their cached state
# after a hardware/API edit. Generate a local copy; never modify the SDK checkout.
set(VTR_AU_WRAPPER "${CMAKE_CURRENT_BINARY_DIR}/vtr-au-wrapper")
file(MAKE_DIRECTORY "${VTR_AU_WRAPPER}")
foreach(name auwrapper.h auwrapper.mm aucocoaview.h aucocoaview.mm NSDataIBStream.h NSDataIBStream.mm)
 file(READ "${AU_WRAPPER}/${name}" source)
 if(name STREQUAL "auwrapper.h")
  set(before "public IComponentHandler, public ITimerCallback")
  set(after "public IComponentHandler, public IComponentHandler2, public ITimerCallback")
  string(FIND "${source}" "${before}" found)
  if(found EQUAL -1)
   message(FATAL_ERROR "Pinned AU wrapper interface changed; review the state notification adaptation")
  endif()
  string(REPLACE "${before}" "${after}" source "${source}")
  string(REPLACE "//---from ITimerCallback" [=[//---from IComponentHandler2 (Virus TI Rework adaptation)
 tresult PLUGIN_API setDirty (TBool state) SMTG_OVERRIDE;
 tresult PLUGIN_API requestOpenEditor (FIDString) SMTG_OVERRIDE { return kNotImplemented; }
 tresult PLUGIN_API startGroupEdit () SMTG_OVERRIDE { return kNotImplemented; }
 tresult PLUGIN_API finishGroupEdit () SMTG_OVERRIDE { return kNotImplemented; }

 //---from ITimerCallback]=] source "${source}")
 elseif(name STREQUAL "auwrapper.mm")
  set(before "IMPLEMENT_FUNKNOWN_METHODS (AUWrapper, IComponentHandler, IComponentHandler::iid)")
  string(FIND "${source}" "${before}" found)
  if(found EQUAL -1)
   message(FATAL_ERROR "Pinned AU wrapper queryInterface changed; review the state notification adaptation")
  endif()
  string(REPLACE "${before}" [=[IMPLEMENT_REFCOUNT (AUWrapper)
tresult PLUGIN_API AUWrapper::queryInterface (const TUID iid, void** obj)
{
 QUERY_INTERFACE (iid, obj, FUnknown::iid, IComponentHandler)
 QUERY_INTERFACE (iid, obj, IComponentHandler::iid, IComponentHandler)
 QUERY_INTERFACE (iid, obj, IComponentHandler2::iid, IComponentHandler2)
 *obj = nullptr;
 return kNoInterface;
}
tresult PLUGIN_API AUWrapper::setDirty (TBool state)
{
 if (state)
  PropertyChanged (kAudioUnitProperty_ClassInfo, kAudioUnitScope_Global, 0);
 return kResultOk;
}]=] source "${source}")
  # A failed snapshot must fail the AU save instead of publishing an empty
  # Processor State and overwriting a host's last usable project state.
  string(REPLACE "[processorData setLength:0];" [=[[processorData setLength:0];
   CFRelease (*outData);
   *outData = nullptr;
   return kAudioUnitErr_CannotDoInCurrentContext;]=] source "${source}")
  string(REPLACE "FUnknownPtr<IComponent> (audioProcessor)->setState (&stream);" [=[if (FUnknownPtr<IComponent> (audioProcessor)->setState (&stream) != kResultOk)
    return kAudioUnitErr_InvalidPropertyValue;]=] source "${source}")
 endif()
 file(WRITE "${VTR_AU_WRAPPER}/${name}" "${source}")
endforeach()
