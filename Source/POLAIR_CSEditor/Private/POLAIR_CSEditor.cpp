#include "POLAIR_CSEditor.h"

#define LOCTEXT_NAMESPACE "FPOLAIR_CSEditorModule"

void FPOLAIR_CSEditorModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uproject file
}

void FPOLAIR_CSEditorModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FPOLAIR_CSEditorModule, POLAIR_CSEditor)