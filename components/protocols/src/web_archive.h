#ifndef WEB_ARCHIVE_H_
#define WEB_ARCHIVE_H_

/**
 * @brief Try find file in web.cpio
 * 
 * @param name file name
 * @param content file content pointer
 * @return size_t file size if found file, otherwise 0
 */
size_t web_archive_find(const char *name, char **content);

#endif /* WEB_ARCHIVE_H_ */