#if HAS_LIBCLANG

#include <wx/app.h>
#include "clang_pch_maker_thread.h"
#include <wx/thread.h>
#include <wx/stdpaths.h>
#include <wx/regex.h>
#include <wx/tokenzr.h>
#include "y.tab.h"
#include "cpp_scanner.h"
#include "file_logger.h"
#include "globals.h"
#include "procutils.h"
#include "fileextmanager.h"
#include <wx/xrc/xmlres.h>

#define cstr(x) x.mb_str(wxConvUTF8).data()

const wxEventType wxEVT_CLANG_PCH_CACHE_STARTED = XRCID("clang_pch_cache_started");
const wxEventType wxEVT_CLANG_PCH_CACHE_ENDED   = XRCID("clang_pch_cache_ended");

////////////////////////////////////////////////////////////////////////////
// Internal class used for traversing the macro found in a translation UNIT

struct MacroClientData {
	std::set<wxString> macros;
	std::set<wxString> interestingMacros;
	wxString           filename;

	wxString intersect() const {
		std::set<wxString> resultSet;
		std::set<wxString>::const_iterator iter = this->interestingMacros.begin();
		for(; iter != this->interestingMacros.end(); iter++) {
			if(this->macros.find(*iter) != this->macros.end()) {
				// this macro exists in both lists
				resultSet.insert(*iter);
			}
		}

		wxString macrosAsStr;
		std::set<wxString>::const_iterator it = resultSet.begin();
		for(; it != resultSet.end(); it++) {
			macrosAsStr << (*it) << wxT(" ");
		}
		return macrosAsStr;
	}
};

enum CXChildVisitResult MacrosCallback(CXCursor cursor,
                                       CXCursor parent,
                                       CXClientData clientData)
{
	// Get all macros here...
	if(cursor.kind == CXCursor_MacroDefinition) {

		// Dont collect macro defined in this file
		CXSourceLocation loc = clang_getCursorLocation(cursor);
		CXFile file;
		unsigned line, col, off;
		clang_getSpellingLocation(loc, &file, &line, &col, &off);

		CXString strFileName = clang_getFileName(file);
		wxFileName fn(clang_getCString(strFileName));
		clang_disposeString(strFileName);
		MacroClientData *cd = (MacroClientData*)clientData;

		if(cd->filename != fn.GetFullPath()) {
			CXString displayName = clang_getCursorDisplayName(cursor);
			cd->macros.insert(clang_getCString(displayName));
			clang_disposeString(displayName);
		}

	}
	return CXChildVisit_Continue;
}

static void printDiagnosticsToLog(CXTranslationUnit TU)
{
	//// Report diagnostics to the log file
	const unsigned diagCount = clang_getNumDiagnostics(TU);
	for(unsigned i=0; i<diagCount; i++) {
		CXDiagnostic diag     = clang_getDiagnostic(TU, i);
		CXString diagStr      = clang_formatDiagnostic(diag, clang_defaultDiagnosticDisplayOptions());
		wxString wxDiagString = wxString(clang_getCString(diagStr), wxConvUTF8);
		if(!wxDiagString.Contains(wxT("'dllimport' attribute"))) {
			CL_DEBUG(wxT("Diagnostic: %s"), wxDiagString.c_str());

		}
		clang_disposeString(diagStr);
		clang_disposeDiagnostic(diag);
	}
}

ClangWorkerThread::ClangWorkerThread()
{
}

ClangWorkerThread::~ClangWorkerThread()
{
}

void ClangWorkerThread::ProcessRequest(ThreadRequest* request)
{
	ClangThreadRequest *task = dynamic_cast<ClangThreadRequest*>( request );
	if( !task ) {
		wxCommandEvent eEnd(wxEVT_CLANG_PCH_CACHE_ENDED);
		eEnd.SetClientData(NULL);
		wxTheApp->AddPendingEvent(eEnd);
		return;
	}
	
	// Send start event
	wxCommandEvent e(wxEVT_CLANG_PCH_CACHE_STARTED);
	wxTheApp->AddPendingEvent(e);
	

	FileExtManager::FileType type = FileExtManager::GetType(task->GetFileName());
	bool isSource = (type == FileExtManager::TypeSourceC || type == FileExtManager::TypeSourceCpp);

	std::string c_dirtyBuffer = cstr(task->GetDirtyBuffer());
	std::string c_filename    = cstr(task->GetFileName());

	CXUnsavedFile unsavedFile = { c_filename.c_str(), c_dirtyBuffer.c_str(), c_dirtyBuffer.length() };
	CXTranslationUnit TU = findEntry(task->GetFileName());

	if(!TU) {
		int argc(0);
		char **argv = MakeCommandLine(task->GetCompilationArgs(), argc, !isSource);

		for(int i=0; i<argc; i++) {
			CL_DEBUG(wxT("Command Line Argument: %s"), wxString(argv[i], wxConvUTF8).c_str());
		}

		CL_DEBUG(wxT("Calling clang_parseTranslationUnit..."));
		// First time, need to create it
		TU = clang_parseTranslationUnit(task->GetIndex(),
		                                c_filename.c_str(),
		                                argv,
		                                argc,
		                                NULL, 0, CXTranslationUnit_CXXPrecompiledPreamble
		                                | CXTranslationUnit_CacheCompletionResults
		                                | CXTranslationUnit_CXXChainedPCH
		                                | CXTranslationUnit_PrecompiledPreamble
		                                | CXTranslationUnit_Incomplete
										| CXTranslationUnit_DetailedPreprocessingRecord);

		CL_DEBUG(wxT("Calling clang_parseTranslationUnit... done"));
		for(int i=0; i<argc; i++) {
			free(argv[i]);
		}
		delete [] argv;

		if(TU) {

			CL_DEBUG(wxT("Calling clang_reparseTranslationUnit..."));
			clang_reparseTranslationUnit(TU, 0, NULL, clang_defaultReparseOptions(TU));
			CL_DEBUG(wxT("Calling clang_reparseTranslationUnit... done"));

		} else {

			CL_DEBUG(wxT("Failed to parse Translation UNIT..."));
			wxCommandEvent eEnd(wxEVT_CLANG_PCH_CACHE_ENDED);
			eEnd.SetClientData(NULL);
			wxTheApp->AddPendingEvent(eEnd);
			return;
		}
	}

	//
	DoCacheResult(TU, task->GetFileName());

	wxCommandEvent eEnd(wxEVT_CLANG_PCH_CACHE_ENDED);
	ClangThreadReply *reply = new ClangThreadReply;
	reply->context    = task->GetContext();
	reply->filterWord = task->GetFilterWord();
	reply->filename   = task->GetFileName().c_str();
	reply->results    = NULL;

	if(task->GetContext() != CTX_CachePCH && task->GetContext() != CTX_Macros) {
#if 0
		//CL_DEBUG(wxT("Calling clang_reparseTranslationUnit..."));
		//clang_reparseTranslationUnit(TU, 0, NULL, clang_defaultReparseOptions(TU));
		//CL_DEBUG(wxT("Calling clang_reparseTranslationUnit... done"));
#endif
		CL_DEBUG(wxT("Calling clang_codeCompleteAt..."));
		CL_DEBUG(wxT("Location: %s:%u:%u"), task->GetFileName().c_str(), task->GetLine(), task->GetColumn());
		// Do the code-completion
		reply->results = clang_codeCompleteAt(TU,
		                                      cstr(task->GetFileName()),
		                                      task->GetLine(),
		                                      task->GetColumn(),
		                                      &unsavedFile,
		                                      1,
		                                      clang_defaultCodeCompleteOptions());

		CL_DEBUG(wxT("Calling clang_codeCompleteAt... done"));
		if(reply->results) {
			CL_DEBUG(wxT("Found %d matches"), reply->results->NumResults);

		}
		if(reply->results && reply->results->NumResults == 0) {
			printDiagnosticsToLog(TU);
		}
		
	} else if(task->GetContext() == CTX_Macros) {
		
		MacroClientData clientData;
		clientData.filename = reply->filename;

		CL_DEBUG(wxT("Traversing TU..."));
		CXCursorVisitor visitor = MacrosCallback;
		clang_visitChildren(clang_getTranslationUnitCursor(TU), visitor, (CXClientData)&clientData);

		clientData.interestingMacros = DoGetUsedMacros(reply->filename);
	
		
		CL_DEBUG(wxT("Traversing TU...done"));
		CL_DEBUG(wxT("Collected %d Macros"), clientData.macros.size());
		CL_DEBUG(wxT("Collected %d Interesting Macros"), clientData.interestingMacros.size());

		wxString macros = clientData.intersect();
		CL_DEBUG(wxT("The following macros will be passed to scintilla: %s"), macros.c_str());
		reply->macrosAsString = macros.c_str(); // Make sure we create a new copy and not using ref-count
	}

	eEnd.SetClientData(reply);
	wxTheApp->AddPendingEvent(eEnd);
}

std::set<wxString> ClangWorkerThread::DoGetUsedMacros(const wxString &filename)
{
	std::set<wxString> pps;
	static wxRegEx reMacro(wxT("#[ \t]*((if)|(elif)|(ifdef)|(ifndef))[ \t]*"));
	wxString fileContent;
	if(!ReadFileWithConversion(filename, fileContent)) {
		return pps;
	}
	
	CppScannerPtr scanner(new CppScanner());
	wxArrayString lines = wxStringTokenize(fileContent, wxT("\r\n"));
	for(size_t i=0; i<lines.GetCount(); i++) {
		wxString line = lines.Item(i).Trim(false);
		if(line.StartsWith(wxT("#")) && reMacro.IsValid() && reMacro.Matches(line)) {
			// Macro line
			wxString match = reMacro.GetMatch(line, 0);
			wxString ppLine = line.Mid(match.Len());

			scanner->Reset();
			scanner->SetText(cstr(ppLine));
			int type(0);
			while( (type = scanner->yylex()) != 0 ) {
				if(type == IDENTIFIER) {
					wxString intMacro = wxString(scanner->YYText(), wxConvUTF8);
					CL_DEBUG1(wxT("Found interesting macro: %s"), intMacro.c_str());
					pps.insert(intMacro);
				}
			}
		}
	}
	
	return pps;
}

CXTranslationUnit ClangWorkerThread::findEntry(const wxString& filename)
{
	wxCriticalSectionLocker locker(m_cs);
	CXTranslationUnit TU = m_cache.GetPCH(filename);
	return TU;
}

void ClangWorkerThread::DoCacheResult(CXTranslationUnit TU, const wxString &filename)
{
	wxCriticalSectionLocker locker(m_cs);
	m_cache.AddPCH( filename, TU);
	CL_DEBUG(wxT("caching Translation Unit file: %s"), filename.c_str());
	CL_DEBUG(wxT(" ==========> [ ClangPchMakerThread ] PCH creation ended successfully <=============="));
}

void ClangWorkerThread::ClearCache()
{
	wxCriticalSectionLocker locker(m_cs);
	m_cache.Clear();
}

bool ClangWorkerThread::IsCacheEmpty()
{
	wxCriticalSectionLocker locker(m_cs);
	return m_cache.IsEmpty();
}

char** ClangWorkerThread::MakeCommandLine(const wxString& command, int& argc, bool isHeader)
{
	wxArrayString tokens = wxStringTokenize(command, wxT(" \t\n\r"), wxTOKEN_STRTOK);
	if(isHeader) {
		tokens.Add(wxT("-x"));
		tokens.Add(wxT("c++-header"));
	}
	tokens.Add(wxT("-ferror-limit=1000"));
	argc = tokens.GetCount();
	char** argv = new char*[argc];

	if(tokens.IsEmpty())
		return argv;

	for(size_t i=0; i<tokens.GetCount(); i++) {
		argv[i] = strdup(tokens.Item(i).mb_str(wxConvUTF8).data());
	}
	return argv;
}

#endif // HAS_LIBCLANG
