//===--- linker.h - Linker Interface -----------------------------*- C++ -*-===//
//
// PawLang Compiler - Linker
//
//===----------------------------------------------------------------------===//

#ifndef PAW_LINKER_H
#define PAW_LINKER_H

#include <string>
#include <vector>

namespace pawc {

/// Linker - linker wrapper
///
/// Responsible for linking object files into executable
/// Uses LLD as underlying linker
class Linker {
public:
    Linker();
    
    /// linkobjectfilegeneratecanexecutefile
    /// \param object_files objectfilelist
    /// \param output_file outputcanexecutefilename
    /// \param library_paths librarysearchpath
    /// \param libraries needlinkof/thelibrary
    /// \return true indicates success
    bool link(const std::vector<std::string>& object_files,
              const std::string& output_file,
              const std::vector<std::string>& library_paths = {},
              const std::vector<std::string>& libraries = {});
    
    /// setruntimelibrarypath
    void setRuntimePath(const std::string& path) { runtime_path_ = path; }
    
    /// setdetailedoutput
    void setVerbose(bool v) { verbose_ = v; }
    
    /// geterrorinfo
    std::string getError() const { return error_; }
    
private:
    std::string runtime_path_;
    std::string error_;
    bool verbose_ = false;
    
    /// Call system linker
    bool invokeSystemLinker(const std::vector<std::string>& args);
    
    /// build/constructlinkcommand
    std::vector<std::string> buildLinkCommand(
        const std::vector<std::string>& object_files,
        const std::string& output_file,
        const std::vector<std::string>& library_paths,
        const std::vector<std::string>& libraries);
};

} // namespace pawc

#endif // PAW_LINKER_H

