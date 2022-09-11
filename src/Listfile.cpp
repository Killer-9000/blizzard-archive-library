#include <Listfile.hpp>
#include <Exception.hpp>
#include <ClientData.hpp>
#include <fstream>
#include <sstream>
#include <cstdint>
#include <intrin.h>

using namespace BlizzardArchive::Listfile;

FileKey::FileKey()
: _file_data_id(0)
, _file_path("")
{
}

FileKey::FileKey(std::string const& filepath, std::uint32_t file_data_id)
: _file_data_id(file_data_id)
, _file_path(ClientData::normalizeFilenameInternal(filepath))
{}

FileKey::FileKey(std::string const& filepath, Listfile* listfile)
  : _file_path(ClientData::normalizeFilenameInternal(filepath))
{
  if (listfile)
  {
    deduceOtherComponent(listfile);
  }

}

FileKey::FileKey(const char* filepath, Listfile* listfile)
: _file_path(ClientData::normalizeFilenameInternal(filepath))
{
  if (listfile)
  {
    deduceOtherComponent(listfile);
  }
}

FileKey::FileKey(const char* filepath, std::uint32_t file_data_id)
  : _file_path(ClientData::normalizeFilenameInternal(filepath))
  , _file_data_id(file_data_id)
{}


FileKey::FileKey(std::uint32_t file_data_id, Listfile* listfile)
  : _file_data_id(file_data_id)
{
  if (listfile)
  {
    deduceOtherComponent(listfile);
  }
}

void Listfile::initFromCSV(std::string const& listfile_path)
{
  // If listfile is already allocated, free it.
  if (_listfile) free(_listfile);

  // Open the listfile for reading.
  FILE* file = fopen(listfile_path.c_str(), "rb");

  if (!file)
  {
    throw Exceptions::Listfile::ListfileNotFoundError();
    return;
  }

  // Get size, and allocate memory and read contents.
  fseek(file, 0, SEEK_END);
  long fileSize = ftell(file);
  fseek(file, 0, SEEK_SET);

  _listfile = (char*)malloc((size_t)ceil((float)fileSize / sizeof(__m128i)) * sizeof(__m128i));

  if (!_listfile)
  {
    throw std::exception("Failed to allocate listfile.");
    return;
  }

  if (fread(_listfile, 1, fileSize, file) != fileSize)
  {
    throw std::exception("Failed to read listfile contents.");
    return;
  }

  // Quickly cleanup the contents for processing.
  static const __m128i mmSlashs = _mm_set1_epi8('\\');
  static const __m128i mmReturns = _mm_set1_epi8('\r');
  static const __m128i mmNewline = _mm_set1_epi8('\n');
  static const __m128i mmUpper = _mm_set1_epi8('Z');
  static const __m128i mmLower = _mm_set1_epi8('A');
  static const __m128i mmDiff = _mm_set1_epi8('a' - 'A');
  __m128i mmChars;
  __m128i mmMask;
  char* start = _listfile;
  char* end = _listfile + fileSize;
  for (; start < end; start += sizeof(__m128i))
  {
    // Load chars into 128bit value.
    mmChars = _mm_loadu_epi8(start);

    // Check for uppercase
    // Set mask to 1
    // Anything less than lower should be 0
    // Anything more than upper should be 0
    mmMask = _mm_cmpeq_epi8(
      _mm_cmpgt_epi8(mmLower, mmChars),
      _mm_cmpgt_epi8(mmChars, mmUpper)
    );
    mmChars = _mm_blendv_epi8(mmChars, _mm_adds_epi8(mmChars, mmDiff), mmMask);

    // Check for backslashes.
    mmMask = _mm_cmpeq_epi8(mmChars, mmSlashs);
    mmChars = _mm_blendv_epi8(mmChars, _mm_set1_epi8('/'), mmMask);

    // Check for \r,\n and replace with null.
    mmMask = _mm_cmpeq_epi8(mmChars, mmReturns);
    mmChars = _mm_blendv_epi8(mmChars, _mm_set1_epi8('\0'), mmMask);
    mmMask = _mm_cmpeq_epi8(mmChars, mmNewline);
    mmChars = _mm_blendv_epi8(mmChars, _mm_set1_epi8('\0'), mmMask);

    // Load 128bit value into chars.
    _mm_storeu_epi8(start, mmChars);
  }

  // Get line count for pre-allocation.
  size_t lineCount = 0;
  for (char* c = _listfile; c < _listfile + fileSize; c++)
    if (*c == '\0') lineCount++;
  _path_to_fdid.reserve(lineCount);
  _fdid_to_path.reserve(lineCount);

  // Go line by line, mapping data.
  char* forward = _listfile;
  const char* current = _listfile;
  char* colonPos = nullptr;
  int uid;
  while (forward < end)
  {
    if (*forward == ';')
      colonPos = forward;
    else if (*forward == '\0')
    {
      // This warning is unjustified, since colon should be before null.
      //   1234;testfile.blp\0
#pragma warning (disable : 6001 6011)
      *colonPos = '\0';
#pragma warning (default : 6001 6011)
      uid = atoi(current);
      if (!_path_to_fdid.contains(current)) [[unlikely]]
      {
        _path_to_fdid[colonPos + 1] = uid;
        _fdid_to_path[uid] = colonPos + 1;
      }
      while (*forward == '\0' && forward < end) forward++;
      current = forward;
    }
    forward++;
  }
}

void Listfile::initFromFileList(char* listfileData, size_t listfileSize)
{
  // TODO: This needs to be stored as an array of pointers, or move cleanup to MPQArchive responsability.
  if (_listfile) free(_listfile);

  if (listfileSize <= 2)
    return;

  listfileSize = (size_t)ceil((float)listfileSize / sizeof(__m128i)) * sizeof(__m128i);
  listfileData = (char*)realloc(listfileData, listfileSize);

  if (!listfileData)
  {
    free(listfileData);
    throw std::exception("Failed to reallocate listfile.");
    return;
  }

  static const __m128i mmSlashs = _mm_set1_epi8('\\');
  static const __m128i mmReturns = _mm_set1_epi8('\r');
  static const __m128i mmNewline = _mm_set1_epi8('\n');
  static const __m128i mmUpper = _mm_set1_epi8('Z');
  static const __m128i mmLower = _mm_set1_epi8('A');
  static const __m128i mmDiff = _mm_set1_epi8('a' - 'A');
  __m128i mmChars;
  __m128i mmMask;
  char* start = listfileData;
  char* end = listfileData + listfileSize;
  for (; start < end; start += sizeof(__m128i))
  {
    // Load chars into 128bit value.
    mmChars = _mm_loadu_epi8(start);

    // Check for uppercase
    // Set mask to 1
    // Anything less than lower should be 0
    // Anything more than upper should be 0
    mmMask = _mm_cmpeq_epi8(
      _mm_cmpgt_epi8(mmLower, mmChars),
      _mm_cmpgt_epi8(mmChars, mmUpper)
    );
    mmChars = _mm_blendv_epi8(mmChars, _mm_adds_epi8(mmChars, mmDiff), mmMask);

    // Check for backslashes.
    mmMask = _mm_cmpeq_epi8(mmChars, mmSlashs);
    mmChars = _mm_blendv_epi8(mmChars, _mm_set1_epi8('/'), mmMask);

    // Check for \r,\n and replace with null.
    mmMask = _mm_cmpeq_epi8(mmChars, mmReturns);
    mmChars = _mm_blendv_epi8(mmChars, _mm_set1_epi8('\0'), mmMask);
    mmMask = _mm_cmpeq_epi8(mmChars, mmNewline);
    mmChars = _mm_blendv_epi8(mmChars, _mm_set1_epi8('\0'), mmMask);

    // Load 128bit value into chars.
    _mm_storeu_epi8(start, mmChars);
  }

  size_t lineCount = 0;
  for (char* c = listfileData; c < listfileData + listfileSize; c++)
    if (*c == '\0') lineCount++;
  _path_to_fdid.reserve(lineCount);

  char* forward = listfileData;
  const char* current = listfileData;
  while (forward < end)
  {
    if (*forward == '\0')
    {
      if (!_path_to_fdid.contains(current))
        _path_to_fdid[current] = 0;
      while (*forward == '\0' && forward < end) forward++;
      current = forward;
    }
    forward++;
  }
}

std::uint32_t Listfile::getFileDataID(std::string const& filename) const
{
  auto it = _path_to_fdid.find(filename.c_str());
  return (it != _path_to_fdid.end()) ? it->second : 0;
}

std::string_view Listfile::getPath(std::uint32_t file_data_id) const
{
  auto it = _fdid_to_path.find(file_data_id);
  return (it != _fdid_to_path.end()) ? it->second : "";
}

bool FileKey::deduceOtherComponent(const Listfile* listfile)
{
  if (hasFileDataID() && !hasFilepath())
  {
    std::string_view path = listfile->getPath(fileDataID());

    if (path.empty())
    {
      return false;
    }

    _file_path = path;
    return true;

  }
  else if (hasFilepath() && !hasFileDataID())
  {
    std::uint32_t fdid = listfile->getFileDataID(filepath());

    if (!fdid)
    {
      return false;
    }

    _file_data_id = fdid;
    return true;
  }

  return false;
}

bool FileKey::operator==(const FileKey& rhs) const
{
  if (hasFileDataID() && rhs.hasFileDataID())
  {
    return _file_data_id == rhs.fileDataID();
  }
  else if (hasFilepath() && rhs.hasFilepath())
  {
    return filepath() == rhs.filepath();
  }

  return false;
}

FileKey::FileKey(FileKey&& other) noexcept
{
  std::swap(_file_data_id, other._file_data_id);
  std::swap(_file_path, other._file_path);
}

std::string FileKey::stringRepr() const
{
  return hasFilepath() ? _file_path.value() : std::to_string(_file_data_id);
}

bool FileKey::operator<(const FileKey& rhs) const
{
  if (hasFileDataID() && rhs.hasFileDataID())
  {
    return _file_data_id < rhs.fileDataID();
  }
  else if (hasFilepath() && rhs.hasFilepath())
  {
    return filepath() < rhs.filepath();
  }

  return false;
}

FileKey& FileKey::operator=(FileKey&& other) noexcept
{
  std::swap(_file_data_id, other._file_data_id);
  std::swap(_file_path, other._file_path);
  return *this;
}

FileKey::FileKey(FileKey const& other)
: _file_data_id(other._file_data_id)
, _file_path(other._file_path)
{
}

FileKey& FileKey::operator= (FileKey const& other)
{
  _file_data_id = other._file_data_id;
  _file_path = other._file_path;

  return *this;
}

