#include "update_manager.hpp"

#include <cstdio>

#include <iostream>
#include <fstream>
#include <cstring>

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <cerrno>
#include <utime.h>

#include <curl/curl.h>

#include "json.hpp"

#include "gui.hpp"

#define EDIZON_URL "http://werwolv.net"

using namespace nlohmann;

CURL *curl;

UpdateManager::UpdateManager(u64 titleID) {
  curl_global_init(CURL_GLOBAL_DEFAULT);
  curl = curl_easy_init();

  if (!curl)
    printf("Curl initialization failed!\n");
}

UpdateManager::~UpdateManager() {
  curl_easy_cleanup(curl);
  curl_global_cleanup();
}

size_t writeToString(void *contents, size_t size, size_t nmemb, void *userp){
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

size_t writeToFile(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    size_t written = fwrite(ptr, size, nmemb, stream);
    return written;
}

void deleteFile(std::string path)
{
  printf("Deleting %s.\n", path.c_str());

  remove(path.c_str());
}

void updateFile(std::string path)
{
  printf("Updating %s.\n", path.c_str());

  std::string url = EDIZON_URL;
  url.append(path);

  FILE* fp;
  if (path.compare("/EdiZon/EdiZon.nro") == 0)
  {
    deleteFile("/switch/EdiZon.nro");
    mkdir("/switch/EdiZon", 0777);
    path = "/switch/EdiZon/EdiZon.nro";
  }
  fp = fopen(path.c_str(), "wb");

  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeToFile);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);

  CURLcode res = curl_easy_perform(curl);

  if (res != CURLE_OK)
    printf("2nd CURL perform failed: %s\n", curl_easy_strerror(res));

  fclose(fp);
}

bool UpdateManager::checkUpdate()
{
  if (!curl)
    return false;

  CURLcode res;
  std::string str;

  curl_easy_setopt(curl, CURLOPT_URL, EDIZON_URL "/EdiZon/versionlist.php");
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeToString);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &str);

  res = curl_easy_perform(curl);

  if (res != CURLE_OK) {
    printf("1st CURL perform failed: %s\n", curl_easy_strerror(res));
    return false;
  }

  printf("Returned data: %s\n", str.c_str());
  if (str.compare(0, 1, "{") != 0)
  {
    printf("Invalid downloaded JSON!\n");
    return false;
  }

  json remote = json::parse(str);

  mkdir("/EdiZon", 0777);
  mkdir("/EdiZon/editor", 0777);
  mkdir("/EdiZon/editor/scripts", 0777);
  mkdir("/EdiZon/editor/scripts/lib", 0777);

  bool updatedFile = false;

	std::ifstream i("/EdiZon/editor/update.json");
	if (!i.is_open())
	{
		printf("File didn't exist, will create it.\n");
		for (json::iterator it = remote.begin(); it != remote.end(); ++it)
			updateFile(it.key());
		updatedFile = true;
	}
	else
	{
		printf("File opened.\n");
		std::string buf;
		std::getline(i, buf);
		if (buf.size() == 0) return false;
		printf("Read from SD card: %s\n", buf.c_str());
    if (buf.compare(0, 1, "{") != 0)
    {
      printf("Invalid JSON on SD card!\n");
      deleteFile("/EdiZon/editor/update.json");
      return false;
    }
		auto local = json::parse(buf);
		i.close();

		for (json::iterator it = remote.begin(); it != remote.end(); ++it)
		{
			if (local.find(it.key()) == local.end() || local[it.key()] < remote[it.key()])
			{
				updateFile(it.key());
				updatedFile = true;
			}
		}

		for (json::iterator it = local.begin(); it != local.end(); ++it)
		{
			if (remote.find(it.key()) == remote.end())
				deleteFile(it.key());
		}
	}

	if (updatedFile)
	{
		printf("Writing updated file to SD.\n");
		std::ofstream o("/EdiZon/editor/update.json");
		if (o.is_open()) {
			o << str;
			o.close();
		}
		else
			printf("Error while opening the output file.\n");
	}

  return updatedFile;
}