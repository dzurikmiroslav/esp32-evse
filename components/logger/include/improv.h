#ifndef IMPROV_H_
#define IMPROV_H_

#define IMPROV_PACKET_SIZE 266

/**
 * @brief Handle improv protocol response in file descriptor
 *
 * @param fd
 */
void improv_handle(int fd);

#endif /* IMPROV_H_ */