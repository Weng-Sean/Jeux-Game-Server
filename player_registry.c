#include "player_registry.h"
#include "debug.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <global.h>


/*
 * Initialize a new player registry.
 *
 * @return the newly initialized PLAYER_REGISTRY, or NULL if initialization
 * fails.
 */
PLAYER_REGISTRY *preg_init(void)
{
    debug("%d", __LINE__);

    // Allocate memory for the registry
    PLAYER_REGISTRY *preg = malloc(sizeof(PLAYER_REGISTRY));
    if (preg == NULL) {
        return NULL;  // Failed to allocate memory
    }

    // Initialize the fields of the registry
    preg->head = NULL;
    preg->player_count = 0;
    pthread_mutex_init(&preg->mutex, NULL);
    sem_init(&preg->sem, 0, 0);

    return preg;

}


/*
 * Finalize a player registry, freeing all associated resources.
 *
 * @param cr  The PLAYER_REGISTRY to be finalized, which must not
 * be referenced again.
 */
void preg_fini(PLAYER_REGISTRY *preg)
{
    debug("%d", __LINE__);

    // acquire the registry's mutex lock
    pthread_mutex_lock(&preg->mutex);
    
    // free all players in the registry
    PLAYER_NODE *cur_node = preg->head;
    while (cur_node != NULL) {
        PLAYER_NODE *next_node = cur_node->next;
        player_unref(cur_node->player, "preg_fini");
        free(cur_node->player->name);
        free(cur_node);
        cur_node = next_node;
    }
    
    // free the registry itself
    free(preg);
    
    // release the registry's mutex lock
    pthread_mutex_unlock(&preg->mutex);
}

/*
 * Register a player with a specified user name.  If there is already
 * a player registered under that user name, then the existing registered
 * player is returned, otherwise a new player is created.
 * If an existing player is returned, then its reference count is increased
 * by one to account for the returned pointer.  If a new player is
 * created, then the returned player has reference count equal to two:
 * one count for the pointer retained by the registry and one count for
 * the pointer returned to the caller.
 *
 * @param name  The player's user name, which is copied by this function.
 * @return A pointer to a PLAYER object, in case of success, otherwise NULL.
 *
 */
PLAYER *preg_register(PLAYER_REGISTRY *preg, char *name)
{
    debug("%d", __LINE__);

    pthread_mutex_lock(&preg->mutex);

    // Check if a player with this name already exists
    PLAYER_NODE *curr = preg->head;
    while (curr != NULL) {
        if (strcmp(player_get_name(curr->player), name) == 0) {
            player_ref(curr->player, "returning the player");
            pthread_mutex_unlock(&preg->mutex);
            return curr->player;
        }
        curr = curr->next;
    }



    // If not, create a new player
    PLAYER *new_player = player_create(name);
    if (new_player == NULL) {
        pthread_mutex_unlock(&preg->mutex);
        return NULL;
    }

    player_ref(new_player, "registry"); // One for the registry, one for the caller
    pthread_mutex_init(&new_player->lock, NULL);

    PLAYER_NODE *new_node = malloc(sizeof(PLAYER_NODE));


    new_node->player = new_player;
    new_node->next = preg->head;
    preg->head = new_node;
    preg->player_count++;
    player_ref(new_node->player, "reference being retained by player registry");
    debug("enter here");

    pthread_mutex_unlock(&preg->mutex);

    return new_player;
}
