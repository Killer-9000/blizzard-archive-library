#ifndef BIZZZARDARCHIVE_LISTFILE_HPP
#define BIZZZARDARCHIVE_LISTFILE_HPP

#include <string>
#include <cstdint>
#include <optional>
#include <vector>
#include <unordered_map>
#include <compare>
#include <tsl/robin_map.h>

namespace BlizzardArchive::Listfile
{
  class Listfile
  {
  public:
    Listfile() = default;
    ~Listfile()
    {
      if (_listfile) free(_listfile);
    };

    void initFromCSV(std::string const& listfile_path);
    void initFromFileList(char* listfileData, size_t listfileSize);

    std::uint32_t getFileDataID(std::string const& filename) const;
    std::string_view getPath(std::uint32_t file_data_id) const;

    tsl::robin_map<std::string_view, std::uint32_t> const& pathToFileDataIDMap() const { return _path_to_fdid; };
    tsl::robin_map<std::uint32_t, std::string_view> const& fileDataIDToPathMap() const { return _fdid_to_path; };

  private:
    tsl::robin_map<std::string_view, std::uint32_t> _path_to_fdid;
    tsl::robin_map<std::uint32_t, std::string_view> _fdid_to_path;
    char* _listfile = nullptr;
  };

  class FileKey
  {
  public:
    FileKey();
    FileKey(std::string const& filepath, Listfile* listfile = nullptr);
    FileKey(const char* filepath, Listfile* listfile = nullptr);
    explicit FileKey(std::uint32_t file_data_id, Listfile* listfile = nullptr);
    FileKey(std::string const& filepath, std::uint32_t file_data_id);
    FileKey(const char* filepath, std::uint32_t file_data_id);

    FileKey(FileKey const& other);
    FileKey& operator= (FileKey const& other);

    FileKey(FileKey&& other) noexcept;
    FileKey& operator= (FileKey&& other) noexcept;

    [[nodiscard]]
    bool hasFilepath() const { return _file_path.has_value(); };

    [[nodiscard]]
    bool hasFileDataID() const { return static_cast<bool>(_file_data_id); };

    [[nodiscard]]
    std::string const& filepath() const { return _file_path.value(); };

    [[nodiscard]]
    std::uint32_t fileDataID() const { return _file_data_id; };

    [[nodiscard]]
    std::string stringRepr() const;

    void setFilepath(std::string const& path) { _file_path = path; }
    void setFileDataID(std::uint32_t file_data_id) { _file_data_id = file_data_id; }
    bool deduceOtherComponent(const Listfile* listfile);

    bool operator==(FileKey const& rhs) const;
    bool operator<(FileKey const& rhs) const;

  private:
    std::uint32_t _file_data_id = 0;
    std::optional<std::string> _file_path;
  };
}

#endif // BIZZZARDARCHIVE_LISTFILE_HPP