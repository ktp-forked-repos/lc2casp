#pragma once

#include "clasp/clasp_app.h"
#include "order/config.h"

// Standalone clasp application.
class ClaspConApp : public Clasp::Cli::ClaspAppBase {
public:
    ClaspConApp();
    const char* getName()       const { return "claspcon"; }
	const char* getVersion()    const { return CLASP_VERSION; }
	const char* getUsage()      const { 
		return 
			"[number] [options] [file]\n"
			"Compute at most <number> models (0=all) of the instance given in <file>"; 
	}

protected:
    virtual void        validateOptions(const ProgramOptions::OptionContext& root, const ProgramOptions::ParsedOptions& parsed, const ProgramOptions::ParsedValues& values);
    virtual void        initOptions(ProgramOptions::OptionContext& root);
    virtual Clasp::ProblemType getProblemType();
    virtual void        run(Clasp::ClaspFacade& clasp);
	virtual void        printHelp(const ProgramOptions::OptionContext& root);
private:
    ClaspConApp(const ClaspConApp&);
    ClaspConApp& operator=(const ClaspConApp&);
    order::Config conf_;
};

