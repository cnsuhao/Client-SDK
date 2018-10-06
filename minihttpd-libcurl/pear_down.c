#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
 
#include <curl/curl.h>
 
static size_t write_data(void *ptr, size_t size, size_t nmemb, void *stream)
{
  size_t written = fwrite(ptr, size, nmemb, (FILE *)stream);
  printf("write_data: %d\n", written);
  return written;
}

/* for Pear multi down logic */
static size_t merge_data()
{

  return 0;
}
 
int pear_down(char * url)
{
  CURL *curl_handle;
  static const char *pagefilename = "pear001.mp4";
  FILE *pagefile;

  /*
  if(argc < 2) {
    printf("Usage: %s <URL>\n", argv[0]);
    return 1;
  }
  */
 
  curl_global_init(CURL_GLOBAL_ALL);
 
  /* init the curl session */ 
  curl_handle = curl_easy_init();
 
  /* set URL to get here */ 
  /* curl_easy_setopt(curl_handle, CURLOPT_URL, argv[1]); */
  curl_easy_setopt(curl_handle, CURLOPT_URL, "https://qq.webrtc.win/tv/pear001.mp4");

  /* Switch on full protocol/debug output while testing */ 
  curl_easy_setopt(curl_handle, CURLOPT_VERBOSE, 1L);
 
  /* disable progress meter, set to 0L to enable and disable debug output */ 
  curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, 1L);
 
  /* send all data to this function  */ 
  curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_data);
 
  /* open the file */ 
  pagefile = fopen(pagefilename, "wb");
  if(pagefile) {
 
    /* write the page body to this file handle */ 
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, pagefile);
 
    /* get it! */ 
    curl_easy_perform(curl_handle);
 
    /* close the header file */ 
    fclose(pagefile);
  }
 
  /* cleanup curl stuff */ 
  curl_easy_cleanup(curl_handle);
 
  return 0;
}
