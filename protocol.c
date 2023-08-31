#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "protocol.h"
#include "global.h"
#include "debug.h"

int proto_send_packet(int fd, JEUX_PACKET_HEADER *hdr, void *data)
{
    // Convert multi-byte fields to network byte order
    // hdr->type = htons(hdr->type);
    // hdr->size = htonl(hdr->size);


    debug("sending... fd:%d", fd);
    uint16_t payload_size = ntohs(hdr->size);

    // Write header to the wire
    if (write(fd, (void *)hdr, sizeof(JEUX_PACKET_HEADER)) != sizeof(JEUX_PACKET_HEADER))
    {

        debug("ERROR: %d", __LINE__);
        return -1;
    }

    // Write payload data to the wire
    if (payload_size > 0)
    {
        if (write(fd, data, payload_size) != payload_size)
        {
            debug("ERROR: %d", __LINE__);
            return -1;
        }
    }

    return 0;
}

int proto_recv_packet(int fd, JEUX_PACKET_HEADER *hdr, void **payloadp)
{

    // Read header from the wire
    size_t read_size = read(fd, (void *)hdr, sizeof(JEUX_PACKET_HEADER));

    uint16_t size = ntohs(hdr->size);
    debug("hdr->size: %hu", size);

    if (read_size != sizeof(JEUX_PACKET_HEADER))
    {
        debug("ERROR: %d, %ld", __LINE__, read_size);
        debug("ERROR: %d, %hu", __LINE__, size);

        return -1;
    }

    // Allocate memory for payload data
    if (size > 0)
    {
        *payloadp = malloc(size);
        if (*payloadp == NULL)
        {
            debug("ERROR: %d", __LINE__);
            return -1;
        }

        // Read payload data from the wire
        if (read(fd, (void *)*payloadp, size) != size)
        {
            free(*payloadp);
            debug("ERROR: %d, %hu", __LINE__, size);
            return -1;
        }
        *(((char*)(*payloadp)) + size) =  0;
    }
    else
    {
        *payloadp = NULL;
    }

    return 0;
}
