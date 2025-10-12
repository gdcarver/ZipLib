#include "ZipFile.h"

#include "utils/stream_utils.h"

#include <fstream>
#include <cassert>
#include <stdexcept>
#include <cerrno>   // for errno
#include <cstring>  // for strerror

namespace
{
  std::string GetFilenameFromPath(const std::string& fullPath)
  {
    std::string::size_type dirSeparatorPos;

    if ((dirSeparatorPos = fullPath.find_last_of('/')) != std::string::npos)
    {
      return fullPath.substr(dirSeparatorPos + 1);
    }
    else
    {
      return fullPath;
    }
  }

  std::string MakeTempFilename(const std::string& fileName)
  {
    return fileName + ".tmp";
  }
}

ZipArchive::Ptr ZipFile::Open(const std::string& zipPath)
{
  std::ifstream* zipFile = new std::ifstream();
  zipFile->open(zipPath, std::ios::binary);

  if (!zipFile->is_open())
  {
    // if file does not exist, try to create it
    std::ofstream tmpFile;
    tmpFile.open(zipPath, std::ios::binary);
    tmpFile.close();

    zipFile->open(zipPath, std::ios::binary);

    // if attempt to create file failed, throw an exception
    if (!zipFile->is_open())
    {
      int syserr = errno;
      std::string syserr_msg = strerror(syserr);

      std::string err_msg = "Cannot open zip file: '"+ zipPath + "': " + 
                            "\nOS error: (" + std::to_string(syserr) + ") : " + syserr_msg;
      delete zipFile; // [Cecil] Don't leave in the memory
      throw std::runtime_error(err_msg);
    }
  }

  return ZipArchive::Create(zipFile, true);
}

void ZipFile::Save(ZipArchive::Ptr zipArchive, const std::string& zipPath)
{
  ZipFile::SaveAndClose(zipArchive, zipPath);

  zipArchive = ZipFile::Open(zipPath);
}

void ZipFile::SaveAndClose(ZipArchive::Ptr zipArchive, const std::string& zipPath)
{
  // check if file exist
  std::string tempZipPath = MakeTempFilename(zipPath);
  std::ofstream outZipFile;
  outZipFile.open(tempZipPath, std::ios::binary | std::ios::trunc);

  if (!outZipFile.is_open())
  {
    int syserr = errno;
    std::string syserr_msg = strerror(syserr);
    std::string err_msg = "Cannot save zip file: '"+ tempZipPath + "': " + 
                          "\nOS error: (" + std::to_string(syserr) + ") : " + syserr_msg;
    throw std::runtime_error(err_msg);
  }

  zipArchive->WriteToStream(outZipFile);
  outZipFile.close();

  zipArchive->InternalDestroy();

  remove(zipPath.c_str());
  rename(tempZipPath.c_str(), zipPath.c_str());
}

bool ZipFile::IsInArchive(const std::string& zipPath, const std::string& fileName)
{
  ZipArchive::Ptr zipArchive = ZipFile::Open(zipPath);
  return zipArchive->GetEntry(fileName) != nullptr;
}

void ZipFile::AddFile(const std::string& zipPath, const std::string& fileName, ICompressionMethod::Ptr method)
{
  AddFile(zipPath, fileName, GetFilenameFromPath(fileName), method);
}

void ZipFile::AddFile(const std::string& zipPath, const std::string& fileName, const std::string& inArchiveName, ICompressionMethod::Ptr method)
{
  AddEncryptedFile(zipPath, fileName, inArchiveName, std::string(), method);
}

void ZipFile::AddEncryptedFile(const std::string& zipPath, const std::string& fileName, const std::string& password, ICompressionMethod::Ptr method)
{
  AddEncryptedFile(zipPath, fileName, GetFilenameFromPath(fileName), std::string(), method);
}

void ZipFile::AddEncryptedFile(const std::string& zipPath, const std::string& fileName, const std::string& inArchiveName, const std::string& password, ICompressionMethod::Ptr method)
{
  std::string tmpName = MakeTempFilename(zipPath);

  {
    ZipArchive::Ptr zipArchive = ZipFile::Open(zipPath);

    std::ifstream fileToAdd;
    fileToAdd.open(fileName, std::ios::binary);

    if (!fileToAdd.is_open())
    {
      int syserr = errno;
      std::string syserr_msg = strerror(syserr);
      std::string err_msg = "Cannot open input file: '"+ fileName + "': " + 
                            "\nOS error: (" + std::to_string(syserr) + ") : " + syserr_msg;
      throw std::runtime_error(err_msg);
    }

    auto fileEntry = zipArchive->CreateEntry(inArchiveName);

    if (fileEntry == nullptr)
    {
      //throw std::runtime_error("input file already exist in the archive");
      zipArchive->RemoveEntry(inArchiveName);
      fileEntry = zipArchive->CreateEntry(inArchiveName);
    }

    if (!password.empty())
    {
      fileEntry->SetPassword(password);
      fileEntry->UseDataDescriptor();
    }

    fileEntry->SetCompressionStream(fileToAdd, method);

    //////////////////////////////////////////////////////////////////////////

    std::ofstream outFile;
    outFile.open(tmpName, std::ios::binary);

    if (!outFile.is_open())
    {
      int syserr = errno;
      std::string syserr_msg = strerror(syserr);
      std::string err_msg = "Cannot open output file: '"+ tmpName + "': " + 
                            "\nOS error: (" + std::to_string(syserr) + ") : " + syserr_msg;
      throw std::runtime_error("cannot open output file");
    }

    zipArchive->WriteToStream(outFile);
    outFile.close();
  
    // force closing the input zip stream
  }

  remove(zipPath.c_str());
  rename(tmpName.c_str(), zipPath.c_str());
}

void ZipFile::ExtractFile(const std::string& zipPath, const std::string& fileName)
{
  ExtractFile(zipPath, fileName, GetFilenameFromPath(fileName));
}

void ZipFile::ExtractFile(const std::string& zipPath, const std::string& fileName, const std::string& destinationPath)
{
  ExtractEncryptedFile(zipPath, fileName, destinationPath, std::string());
}

void ZipFile::ExtractEncryptedFile(const std::string& zipPath, const std::string& fileName, const std::string& password)
{
  ExtractEncryptedFile(zipPath, fileName, GetFilenameFromPath(fileName), password);
}

void ZipFile::ExtractEncryptedFile(const std::string& zipPath, const std::string& fileName, const std::string& destinationPath, const std::string& password)
{
  ZipArchive::Ptr zipArchive = ZipFile::Open(zipPath);

  std::ofstream destFile;
  destFile.open(destinationPath, std::ios::binary | std::ios::trunc);

  if (!destFile.is_open())
  {
    int syserr = errno;
    std::string syserr_msg = strerror(syserr);
    std::string err_msg = "Cannot create destination file: '"+ destinationPath + "': " + 
                          "\nOS error: (" + std::to_string(syserr) + ") : " + syserr_msg;
    throw std::runtime_error(err_msg);
  }

  auto entry = zipArchive->GetEntry(fileName);

  if (entry == nullptr)
  {
    std::string err_msg = "File '" + fileName + "' is not contained in zip file '" + zipPath + "'";
    throw std::runtime_error(err_msg);
  }

  if (!password.empty())
  {
    entry->SetPassword(password);
  }

  std::istream* dataStream = entry->GetDecompressionStream();

  if (dataStream == nullptr)
  {
    std::string err_msg = "Cannot extract file '" + fileName + "' from zip file '" + zipPath + "'. Wrong password.";
    throw std::runtime_error(err_msg);
  }

  utils::stream::copy(*dataStream, destFile);

  destFile.flush();
  destFile.close();
}

void ZipFile::RemoveEntry(const std::string& zipPath, const std::string& fileName)
{
  std::string tmpName = MakeTempFilename(zipPath);

  {
    ZipArchive::Ptr zipArchive = ZipFile::Open(zipPath);
    zipArchive->RemoveEntry(fileName);

    //////////////////////////////////////////////////////////////////////////

    std::ofstream outFile;

    outFile.open(tmpName, std::ios::binary);

    if (!outFile.is_open())
    {
      int syserr = errno;
      std::string syserr_msg = strerror(syserr);
      std::string err_msg = "Cannot open output file: '"+ tmpName + "': " + 
                            "\nOS error: (" + std::to_string(syserr) + ") : " + syserr_msg;
      throw std::runtime_error(err_msg);
    }

    zipArchive->WriteToStream(outFile);
    outFile.close();

    // force closing the input zip stream
  }

  remove(zipPath.c_str());
  rename(tmpName.c_str(), zipPath.c_str());
}
