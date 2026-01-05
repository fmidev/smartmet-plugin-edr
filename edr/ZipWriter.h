#pragma once

#ifndef ZIP_WRITER_HPP
#define ZIP_WRITER_HPP

#include <fstream>
#include <string>
#include <vector>
#include <zip.h>

namespace
{

const std::string defaultTmpFilePath = "/var/tmp";

class TmpFile
{
 public:
  TmpFile() = delete;
  TmpFile(const TmpFile &) = delete;
  TmpFile &operator=(const TmpFile &) = delete;

  TmpFile(const std::string &dir, const std::string &fileNameTemplate)
  {
    tmpFile = dir + ((dir.back() != '/') ? "/" : "") + fileNameTemplate;

    std::vector<char> buf(tmpFile.begin(), tmpFile.end());
    buf.push_back('\0');

    int fd = mkstemp(buf.data());
    if (fd == -1)
      throw std::runtime_error("mkstemp failed");

    ::close(fd);

    tmpFile = buf.data();
  }

  ~TmpFile()
  {
    if (!tmpFile.empty())
      ::unlink(tmpFile.c_str());
  }

  const std::string &path() const { return tmpFile; }

 private:
  std::string tmpFile;
};

class ZipSource
{
 public:
  ZipSource() = delete;
  ZipSource(const ZipSource &) = delete;
  ZipSource &operator=(const ZipSource &) = delete;

  ZipSource(zip_t *archive, const std::string &content) : zipSource(nullptr)
  {
    auto buf = std::make_unique<unsigned char[]>(content.size());
    memcpy(buf.get(), content.data(), content.size());

    zipSource = zip_source_buffer(archive, buf.get(), content.size(), 1);
    if (!zipSource)
      throw std::runtime_error("zip_source_buffer failed: " +
                               std::string(zip_error_strerror(zip_get_error(archive))));

    buf.release();
  }

  ~ZipSource()
  {
    if (zipSource)
      zip_source_free(zipSource);
  }

  zip_source_t *get() const { return zipSource; }

  zip_source_t *release()
  {
    zip_source_t *ret = zipSource;
    zipSource = nullptr;

    return ret;
  }

 private:
  zip_source_t *zipSource;
};

class ZipArchive
{
 public:
  ZipArchive() = delete;
  ZipArchive(const ZipArchive &) = delete;
  ZipArchive &operator=(const ZipArchive &) = delete;

  explicit ZipArchive(const std::string &path) : zipArchive(nullptr)
  {
    int err = 0;

    zipArchive = zip_open(path.c_str(), ZIP_CREATE | ZIP_TRUNCATE, &err);
    if (!zipArchive)
    {
      zip_error_t ze;
      zip_error_init_with_code(&ze, err);
      std::string errMsg = zip_error_strerror(&ze);
      zip_error_fini(&ze);

      throw std::runtime_error("zip_open failed: " + errMsg);
    }
  }

  ~ZipArchive()
  {
    if (zipArchive)
    {
      zip_discard(zipArchive);
      zipArchive = nullptr;
    }
  }

  void addFile(const std::string &fileName, const std::string &content)
  {
    ZipSource zipSource(zipArchive, content);

    zip_int64_t idx = zip_file_add(
        zipArchive, fileName.c_str(), zipSource.get(), ZIP_FL_ENC_UTF_8 | ZIP_FL_OVERWRITE);

    if (idx < 0)
      throw std::runtime_error("zip_file_add failed: " +
                               std::string(zip_error_strerror(zip_get_error(zipArchive))));

    zipSource.release();
  }

  void close()
  {
    if (!zipArchive)
      throw std::runtime_error("zip is already closed");

    if (zip_close(zipArchive) < 0)
      throw std::runtime_error("zip_close failed: " +
                               std::string(zip_error_strerror(zip_get_error(zipArchive))));

    zipArchive = nullptr;
  }

 private:
  zip_t *zipArchive;
};

}  // namespace

class ZipWriter
{
 public:
  explicit ZipWriter(const std::string &tmpDirectory = "")
      : tmpFileDir(tmpDirectory.empty() ? defaultTmpFilePath : tmpDirectory)
  {
    if (tmpFileDir.back() != '/')
      tmpFileDir += "/";
  }

  void addFile(const std::string &fileName, const std::string &content)
  {
    if (fileName.empty())
      throw std::runtime_error("addFile(): file name must not be empty");
    if (content.empty())
      throw std::runtime_error("addFile(): file content must not be empty");

    files.emplace_back(fileName, content);
  }

  bool empty() const { return files.empty(); }
  void clear() { files.clear(); }

  std::string createZip()
  {
    if (empty())
      throw std::runtime_error("createZip(): no files");

    TmpFile tmpFile(tmpFileDir, "edr_iwxxm.zip_XXXXXX");
    ZipArchive archive(tmpFile.path());

    for (auto const &file : files)
      archive.addFile(file.first, file.second);

    archive.close();

    std::ifstream in(tmpFile.path(), std::ios::binary | std::ios::ate);
    if (!in)
      throw std::runtime_error("Failed to open created zip for reading");

    std::streamsize size = in.tellg();
    in.seekg(0);

    std::string data(size, '\0');
    if (!in.read(data.data(), size))
      throw std::runtime_error("Failed to read created zip file");

    return data;
  }

 private:
  std::string tmpFileDir;
  std::vector<std::pair<std::string, std::string>> files;
};

#endif
