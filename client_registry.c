#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <semaphore.h>
#include "global.h"
#include "client_registry.h"
#include <string.h>
#include "debug.h"



// static pthread_mutex_t players_mutex = PTHREAD_MUTEX_INITIALIZER;

/*
 * Initialize a new client registry.
 *
 * @return  the newly initialized client registry, or NULL if initialization
 * fails.
 */

CLIENT_REGISTRY *creg_init()
{
    debug("%d", __LINE__);
    CLIENT_REGISTRY *registry = malloc(sizeof(CLIENT_REGISTRY));
    if (registry == NULL)
    {
        return NULL;
    }

    registry->head = NULL;
    registry->client_count = 0;
    pthread_mutex_init(&registry->mutex, NULL);
    sem_init(&registry->sem, 0, 0);

    return registry;
}

/*
 * Register a client file descriptor.
 * If successful, returns a reference to the the newly registered CLIENT,
 * otherwise NULL.  The returned CLIENT has a reference count of one.
 *
 * @param cr  The client registry.
 * @param fd  The file descriptor to be registered.
 * @return a reference to the newly registered CLIENT, if registration
 * is successful, otherwise NULL.
 */


CLIENT *creg_register(CLIENT_REGISTRY *cr, int fd)
{
    debug("%d", __LINE__);

    if (cr->client_count >= MAX_CLIENTS)
    {
        return NULL;
    }

    CLIENT_NODE *new_node = malloc(sizeof(CLIENT_NODE));
    new_node->client = client_create(cr, fd);
    new_node->client->fd = fd;
    new_node->next = NULL;

    debug("Enter here: %d", __LINE__);

    pthread_mutex_lock(&cr->mutex);
    if (cr->head == NULL)
    {
        cr->head = new_node;
    }
    else
    {
        CLIENT_NODE *p = cr->head;
        while (p->next != NULL)
        {
            p = p->next;
        }
        p->next = new_node;
    }
    cr->client_count++;
    pthread_mutex_unlock(&cr->mutex);

    CLIENT_NODE *iter = cr->head;
    while (iter != NULL)
    {
        debug("fd: %d", iter->client->fd);
        iter = iter->next;
    }

    return new_node->client;
}

/*
 * Unregister a CLIENT, removing it from the registry.
 * The client reference count is decreased by one to account for the
 * pointer discarded by the client registry.  If the number of registered
 * clients is now zero, then any threads that are blocked in
 * creg_wait_for_empty() waiting for this situation to occur are allowed
 * to proceed.  It is an error if the CLIENT is not currently registered
 * when this function is called.
 *
 * @param cr  The client registry.
 * @param client  The CLIENT to be unregistered.
 * @return 0  if unregistration succeeds, otherwise -1.
 */
int creg_unregister(CLIENT_REGISTRY *cr, CLIENT *client)
{
    debug("%d", __LINE__);

    int fd = client_get_fd(client);

    pthread_mutex_lock(&cr->mutex);
    CLIENT_NODE *p = cr->head;
    CLIENT_NODE *prev = NULL;
    while (p != NULL)
    {
        if (client_get_fd(p->client) == fd)
        {
            if (prev == NULL)
            {
                cr->head = p->next;
            }
            else
            {
                prev->next = p->next;
            }
            cr->client_count--;
            free(p);
            break;
        }
        prev = p;
        p = p->next;
    }
    pthread_mutex_unlock(&cr->mutex);

    free(client);
    return 0;
}

/*
 * Shut down (using shutdown(2)) all the sockets for connections
 * to currently registered clients.  The clients are not unregistered
 * by this function.  It is intended that the clients will be
 * unregistered by the threads servicing their connections, once
 * those server threads have recognized the EOF on the connection
 * that has resulted from the socket shutdown.
 *
 * @param cr  The client registry.
 */
void creg_shutdown_all(CLIENT_REGISTRY *cr)
{
    debug("%d", __LINE__);

    pthread_mutex_lock(&cr->mutex);
    CLIENT_NODE *p = cr->head;
    while (p != NULL)
    {
        shutdown(p->client->fd, SHUT_RD);
        CLIENT_NODE *tmp = p;
        p = p->next;

        if (client_get_player(p->client) != NULL)
        {
            client_logout(p->client);
        }
        free(tmp->client);
        free(tmp);
    }
    cr->head = NULL;
    cr->client_count = 0;
    sem_post(&cr->sem);
    pthread_mutex_unlock(&cr->mutex);
}

/*
 * A thread calling this function will block in the call until
 * the number of registered clients has reached zero, at which
 * point the function will return.  Note that this function may be
 * called concurrently by an arbitrary number of threads.
 *
 * @param cr  The client registry.
 */

void creg_wait_for_empty(CLIENT_REGISTRY *cr)
{
    debug("%d", __LINE__);

    sem_wait(&cr->sem);
    pthread_mutex_lock(&cr->mutex);
    while (cr->head != NULL)
    {
        shutdown(client_get_fd(cr->head->client), SHUT_RD);
        CLIENT_NODE *tmp = cr->head;
        cr->head = cr->head->next;
        free(tmp);
    }
    pthread_mutex_unlock(&cr->mutex);
}

int creg_client_count(CLIENT_REGISTRY *cr)
{
    return cr->client_count;
}

/*
 * Finalize a client registry, freeing all associated resources.
 * This method should not be called unless there are no currently
 * registered clients.
 *
 * @param cr  The client registry to be finalized, which must not
 * be referenced again.
 */

void creg_fini(CLIENT_REGISTRY *cr)
{
    debug("%d", __LINE__);

    creg_wait_for_empty(cr);

    pthread_mutex_destroy(&cr->mutex);
    sem_destroy(&cr->sem);

    free(cr);
}

/*
 * Return a list of all currently logged in players.  The result is
 * returned as a malloc'ed array of PLAYER pointers, with a NULL
 * pointer marking the end of the array.  It is the caller's
 * responsibility to decrement the reference count of each of the
 * entries and to free the array when it is no longer needed.
 *
 * @param cr  The registry for which the set of usernames is to be
 * obtained.
 * @return the list of players as a NULL-terminated array of pointers.
 */

PLAYER **creg_all_players(CLIENT_REGISTRY *cr)
{
    debug("%d", __LINE__);

    // Lock the mutex to prevent concurrent modification of player data
    pthread_mutex_lock(&(cr->mutex));

    // Create an array of players
    PLAYER **player_list = malloc((cr->client_count + 1) * sizeof(PLAYER *)); // +1 for the NULL terminating pointer

    // Copy the players array from the client registry to the player list array
    CLIENT_NODE *iter = cr->head;
    debug("client_count: %d", cr->client_count);
    int idx = 0;
    for (int i = 0; i < cr->client_count; i++)
    {
        PLAYER *p = client_get_player(iter->client);
        if (p != NULL)
        {
            player_list[idx] = p;
            // Increment reference count since we are returning a pointer to each player
            player_ref(player_list[idx], "reference being added to players list");
            debug("idx: %d, addr: %p, iter_addr: %p, name: %s", idx, player_list[idx], iter, player_get_name(player_list[idx]));
            idx++;
        }
        iter = iter->next;
    }
    // Terminate the list with a NULL pointer

    while (idx <= creg_client_count(cr))
    {
        player_list[idx++] = NULL;
    }

    // Unlock the mutex before returning the player list
    pthread_mutex_unlock(&(cr->mutex));

    return player_list;
    // return NULL;
}

/*
 * Given a username, return the CLIENT that is logged in under that
 * username.  The reference count of the returned CLIENT is
 * incremented by one to account for the reference returned.
 *
 * @param cr  The registry in which the lookup is to be performed.
 * @param user  The username that is to be looked up.
 * @return the CLIENT currently registered under the specified
 * username, if there is one, otherwise NULL.
 */
CLIENT *creg_lookup(CLIENT_REGISTRY *cr, char *user)
{

    // Lock the mutex to prevent concurrent modification of player data
    pthread_mutex_lock(&(cr->mutex));

    debug("%d", __LINE__);

    CLIENT_NODE *iter = cr->head;
    while (iter != NULL && client_get_player(iter->client) != NULL && strcmp(player_get_name(client_get_player(iter->client)), user) != 0)
    {
        iter = iter->next;
    }

    if (iter == NULL){
        debug("NULL ERROR");
        return NULL;
    }
    client_ref(iter->client, "creg_lookup");

    pthread_mutex_unlock(&(cr->mutex));

    return iter->client;
}
