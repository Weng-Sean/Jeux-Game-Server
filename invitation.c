#include <stdlib.h>
#include <string.h>
#include "global.h"
#include "invitation.h"
#include <pthread.h>
#include "debug.h"


/*
 * Create an INVITATION in the OPEN state, containing reference to
 * specified source and target CLIENTs, which cannot be the same CLIENT.
 * The reference counts of the source and target are incremented to reflect
 * the stored references.
 *
 * @param source  The CLIENT that is the source of this INVITATION.
 * @param target  The CLIENT that is the target of this INVITATION.
 * @param source_role  The GAME_ROLE to be played by the source of this INVITATION.
 * @param target_role  The GAME_ROLE to be played by the target of this INVITATION.
 * @return a reference to the newly created INVITATION, if initialization
 * was successful, otherwise NULL.
 */




INVITATION *inv_create(CLIENT *source, CLIENT *target,
		       GAME_ROLE source_role, GAME_ROLE target_role){

    debug("invitation.c");
    // Check that source and target are different
    if (source == target) {
        return NULL;
    }
    // Allocate memory for the new INVITATION object
    INVITATION *inv = malloc(sizeof(INVITATION));
    if (inv == NULL) {
        return NULL;
    }
    // Initialize INVITATION object
    inv->state = INV_OPEN_STATE;
    inv->source = source;
    inv->target = target;
    inv->source_role = source_role;
    inv->target_role = target_role;
    inv->game = NULL;
    inv->ref_count = 1; // one reference held by the invitation registry

    pthread_mutex_init(&(inv->mutex), NULL);
    // Increment reference counts of source and target CLIENTs
    client_ref(source, "invitation");
    client_ref(target, "invitation");
    return inv;
}

/*
 * Increase the reference count on an invitation by one.
 *
 * @param inv  The INVITATION whose reference count is to be increased.
 * @param why  A string describing the reason why the reference count is
 * being increased.  This is used for debugging printout, to help trace
 * the reference counting.
 * @return  The same INVITATION object that was passed as a parameter.
 */
INVITATION *inv_ref(INVITATION *inv, char *why){
    debug("invitation.c");

    pthread_mutex_lock(&inv->mutex);
    inv->ref_count++;
    pthread_mutex_unlock(&inv->mutex);
    return inv;
}

/*
 * Decrease the reference count on an invitation by one.
 * If after decrementing, the reference count has reached zero, then the
 * invitation and its contents are freed.
 *
 * @param inv  The INVITATION whose reference count is to be decreased.
 * @param why  A string describing the reason why the reference count is
 * being decreased.  This is used for debugging printout, to help trace
 * the reference counting.
 *
 */

void inv_unref(INVITATION *inv, char *why){
    debug("invitation.c");

    if (inv == NULL) {
        return;
    }

    pthread_mutex_lock(&inv->mutex);
    inv->ref_count--;

    if (inv->ref_count == 0) {
        pthread_mutex_unlock(&inv->mutex);
        free(inv);
    } else {
        pthread_mutex_unlock(&inv->mutex);
    }
}
/*
 * Get the CLIENT that is the source of an INVITATION.
 * The reference count of the returned CLIENT is NOT incremented,
 * so the CLIENT reference should only be regarded as valid as
 * long as the INVITATION has not been freed.
 *
 * @param inv  The INVITATION to be queried.
 * @return the CLIENT that is the source of the INVITATION.
 */
CLIENT *inv_get_source(INVITATION *inv){
    debug("invitation.c");

    if (inv == NULL) {
        return NULL;
    }
    pthread_mutex_lock(&inv->mutex);
    CLIENT *source = inv->source;
    pthread_mutex_unlock(&inv->mutex);
    return source;
}

/*
 * Get the CLIENT that is the target of an INVITATION.
 * The reference count of the returned CLIENT is NOT incremented,
 * so the CLIENT reference should only be regarded as valid if
 * the INVITATION has not been freed.
 *
 * @param inv  The INVITATION to be queried.
 * @return the CLIENT that is the target of the INVITATION.
 */
CLIENT *inv_get_target(INVITATION *inv){
    debug("invitation.c");

    if (inv == NULL) {
        return NULL;
    }
    pthread_mutex_lock(&inv->mutex);
    CLIENT *target = inv->target;
    pthread_mutex_unlock(&inv->mutex);
    return target;
}

/*
 * Get the GAME_ROLE to be played by the source of an INVITATION.
 *
 * @param inv  The INVITATION to be queried.
 * @return the GAME_ROLE played by the source of the INVITATION.
 */
GAME_ROLE inv_get_source_role(INVITATION *inv){
    debug("invitation.c");

    if (inv == NULL) {
        return NULL_ROLE;
    }
    pthread_mutex_lock(&inv->mutex);
    GAME_ROLE role = inv->source_role;
    pthread_mutex_unlock(&inv->mutex);
    return role;
}

/*
 * Get the GAME_ROLE to be played by the target of an INVITATION.
 *
 * @param inv  The INVITATION to be queried.
 * @return the GAME_ROLE played by the target of the INVITATION.
 */
GAME_ROLE inv_get_target_role(INVITATION *inv){
    debug("invitation.c");

    if (inv == NULL) {
        return NULL_ROLE;
    }
    pthread_mutex_lock(&inv->mutex);
    GAME_ROLE role = inv->target_role;
    pthread_mutex_unlock(&inv->mutex);
    return role;
}

/*
 * Get the GAME (if any) associated with an INVITATION.
 * The reference count of the returned GAME is NOT incremented,
 * so the GAME reference should only be regarded as valid as long
 * as the INVITATION has not been freed.
 *
 * @param inv  The INVITATION to be queried.
 * @return the GAME associated with the INVITATION, if there is one,
 * otherwise NULL.
 */
GAME *inv_get_game(INVITATION *inv){
    debug("invitation.c");

    if (inv == NULL) {
        return NULL;
    }
    pthread_mutex_lock(&inv->mutex);
    GAME *game = inv->game;
    pthread_mutex_unlock(&inv->mutex);
    return game;
}

/*
 * Accept an INVITATION, changing it from the OPEN to the
 * ACCEPTED state, and creating a new GAME.  If the INVITATION was
 * not previously in the the OPEN state then it is an error.
 *
 * @param inv  The INVITATION to be accepted.
 * @return 0 if the INVITATION was successfully accepted, otherwise -1.
 */
int inv_accept(INVITATION *inv){
    debug("invitation.c");

    pthread_mutex_lock(&inv->mutex);
    if (inv->state != INV_OPEN_STATE) {
        pthread_mutex_unlock(&inv->mutex);
        return -1;
    }
    // create new game
    GAME *game = game_create();
    if (game == NULL) {
        pthread_mutex_unlock(&inv->mutex);
        return -1;
    }
    inv->game = game;
    inv->state = INV_ACCEPTED_STATE;
    pthread_mutex_unlock(&inv->mutex);
    return 0;
}

/*
 * Close an INVITATION, changing it from either the OPEN state or the
 * ACCEPTED state to the CLOSED state.  If the INVITATION was not previously
 * in either the OPEN state or the ACCEPTED state, then it is an error.
 * If INVITATION that has a GAME in progress is closed, then the GAME
 * will be resigned by a specified player.
 *
 * @param inv  The INVITATION to be closed.
 * @param role  This parameter identifies the GAME_ROLE of the player that
 * should resign as a result of closing an INVITATION that has a game in
 * progress.  If NULL_ROLE is passed, then the invitation can only be
 * closed if there is no game in progress.
 * @return 0 if the INVITATION was successfully closed, otherwise -1.
 */
int inv_close(INVITATION *inv, GAME_ROLE role){
    debug("invitation.c");

    int ret = -1;
    pthread_mutex_lock(&inv->mutex);

    if (role == NULL_ROLE){
        if (inv->game == NULL){
            inv->state = INV_CLOSED_STATE;
            pthread_mutex_unlock(&inv->mutex);
            return 0;
        }
        else{
            debug("ERROR");
            pthread_mutex_unlock(&inv->mutex);
            return -1;
        }

    }

    if (inv->state == INV_ACCEPTED_STATE || inv->state == INV_OPEN_STATE) {
        // close the invitation
        inv->state = INV_CLOSED_STATE;

        // resign game if there is one in progress
        if (inv->game != NULL) {

            debug("RESIGNING THE GAME");

            // resign the game
            game_resign(inv->game, role);
        }

        ret = 0;
    }

    pthread_mutex_unlock(&inv->mutex);
    return ret;
}

