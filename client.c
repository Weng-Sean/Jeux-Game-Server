#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include "global.h"

#include "client.h"
// #include "invitation.h"
#include "debug.h"
#include <string.h>

/*
 * Create a new CLIENT object with a specified file descriptor with which
 * to communicate with the client.  The returned CLIENT has a reference
 * count of one and is in the logged-out state.
 *
 * @param creg  The client registry in which to create the client.
 * @param fd  File descriptor of a socket to be used for communicating
 * with the client.
 * @return  The newly created CLIENT object, if creation is successful,
 * otherwise NULL.
 */

CLIENT *client_create(CLIENT_REGISTRY *creg, int fd)
{
    debug("client.c");
    CLIENT *client = malloc(sizeof(CLIENT));
    if (client == NULL)
    {
        return NULL;
    }

    client->fd = fd;
    client->player = NULL;
    client->invitations = NULL;
    client->registry = creg;
    pthread_mutex_init(&client->lock, NULL);
    client->refcount = 1;
    client->invitation_id = 0;

    return client;
}

// Increase the reference count on a CLIENT object.
/*
 * Increase the reference count on a CLIENT by one.
 *
 * @param client  The CLIENT whose reference count is to be increased.
 * @param why  A string describing the reason why the reference count is
 * being increased.  This is used for debugging printout, to help trace
 * the reference counting.
 * @return  The same CLIENT that was passed as a parameter.
 */
CLIENT *client_ref(CLIENT *client, char *why)
{
    debug("client.c");

    client->refcount++;

    debug("client_ref returned");
    return client;
}

/*
 * Decrease the reference count on a CLIENT by one.  If after
 * decrementing, the reference count has reached zero, then the CLIENT
 * and its contents are freed.
 *
 * @param client  The CLIENT whose reference count is to be decreased.
 * @param why  A string describing the reason why the reference count is
 * being decreased.  This is used for debugging printout, to help trace
 * the reference counting.
 */
void client_unref(CLIENT *client, char *why)
{
    debug("client.c");

    pthread_mutex_lock(&client->lock);
    client->refcount--;
    if (client->refcount == 0)
    {
        // free client object and release any resources it holds
        client_logout(client);

        creg_unregister(client->registry, client);
        pthread_mutex_destroy(&client->lock);
        free(client);
    }
    pthread_mutex_unlock(&client->lock);
}

/*
 * Log in this CLIENT as a specified PLAYER.
 * The login fails if the CLIENT is already logged in or there is already
 * some other CLIENT that is logged in as the specified PLAYER.
 * Otherwise, the login is successful, the CLIENT is marked as "logged in"
 * and a reference to the PLAYER is retained by it.  In this case,
 * the reference count of the PLAYER is incremented to account for the
 * retained reference.
 *
 * @param CLIENT  The CLIENT that is to be logged in.
 * @param PLAYER  The PLAYER that the CLIENT is to be logged in as.
 * @return 0 if the login operation is successful, otherwise -1.
 */

int client_login(CLIENT *client, PLAYER *player)
{

    debug("client_login");
    debug("client addr: %p", client);
    if (player == NULL)
    {
        debug("player is NULL");
        return -1;
    }

    pthread_mutex_lock(&client->lock);

    debug("client_login");

    // check if client is already logged in
    if (client->player != NULL)
    {
        pthread_mutex_unlock(&client->lock);
        debug("already logged in");
        return -1;
    }

    debug("client_login");
    debug("client_registry == NULL: %d", client->registry == NULL);
    debug("player == NULL: %d", player == NULL);
    debug("player name: %s", player->name);
    debug("BEFORE ASIGNMENT: player == NULL: %d", client->player == NULL);

    // check if the specified player is already logged in by some other client
    CLIENT *other_client = creg_lookup(client->registry, player_get_name(player));

    debug("123");
    if (other_client != NULL && other_client != client)
    {

        pthread_mutex_unlock(&client->lock);
        client_unref(other_client, "player is already logged in by some other client");
        debug("player is already logged in by some other client");
        debug("other client addr: %p", other_client);
        return -1;
    }

    debug("client_login");

    // login is successful
    client->player = player;
    debug("AFTER ASIGNMENT: player == NULL: %d", client->player == NULL);

    debug("player name: %s", player_get_name(client->player));
    player_ref(player, "logging in client");
    pthread_mutex_unlock(&client->lock);

    debug("client_login");

    return 0;
}

/*
 * Log out this CLIENT.  If the client was not logged in, then it is
 * an error.  The reference to the PLAYER that the CLIENT was logged
 * in as is discarded, and its reference count is decremented.  Any
 * INVITATIONs in the client's list are revoked or declined, if
 * possible, any games in progress are resigned, and the invitations
 * are removed from the list of this CLIENT as well as its opponents'.
 *
 * @param client  The CLIENT that is to be logged out.
 * @return 0 if the client was logged in and has been successfully
 * logged out, otherwise -1.
 */

int client_logout(CLIENT *client)
{
    debug("client.c");

    pthread_mutex_lock(&client->lock);

    // check if the client is logged in
    if (client_get_player(client) == NULL)
    {
        pthread_mutex_unlock(&client->lock);
        return -1;
    }

    INVITATION_NODE *inv_node = client->invitations;
    while (inv_node != NULL)
    {
        // revoke or decline any invitations

        if (inv_get_game(inv_node->invitation) == NULL)
        {
            if (inv_get_source((inv_node->invitation)) == client)
            {

                client_revoke_invitation(client, inv_node->id);
            }
            else if (inv_node->invitation->target == client)
            {
                client_remove_invitation(client, inv_node->invitation);
            }
            else
            {
                debug("THE CLINE IS NOT SOURCE AND NOT TARGET!!");
            }
        }
        else
        {
            game_resign(inv_node->invitation->game, inv_node->invitation->source == client ? FIRST_PLAYER_ROLE : SECOND_PLAYER_ROLE);
        }

        // move on to the next invitation
        inv_node = inv_node->next;
    }

    // release the reference to the player
    player_unref(client->player, "logging out client");
    client->player = NULL;

    pthread_mutex_unlock(&client->lock);
    return 0;
}

/*
 * Get the PLAYER for the specified logged-in CLIENT.
 * The reference count on the returned PLAYER is NOT incremented,
 * so the returned reference should only be regarded as valid as long
 * as the CLIENT has not been freed.
 *
 * @param client  The CLIENT from which to get the PLAYER.
 * @return  The PLAYER that the CLIENT is currently logged in as,
 * otherwise NULL if the player is not currently logged in.
 */

PLAYER *client_get_player(CLIENT *client)
{
    debug("client.c");

    // pthread_mutex_lock(&client->lock);
    debug("client.c");

    debug("client addr: %p", client);

    PLAYER *player = client->player;
    // pthread_mutex_unlock(&client->lock);

    if (player == NULL)
    {
        debug("123");
        return NULL;
    }
    debug("player name: %s", player_get_name(player));
    return player;
}

/*
 * Get the file descriptor for the network connection associated with
 * this CLIENT.
 *
 * @param client  The CLIENT for which the file descriptor is to be
 * obtained.
 * @return the file descriptor.
 */
int client_get_fd(CLIENT *client)
{
    debug("client.c");

    pthread_mutex_lock(&client->lock);
    int fd = client->fd;
    pthread_mutex_unlock(&client->lock);

    return fd;
}

/*
 * Send a packet to a client.  Exclusive access to the network connection
 * is obtained for the duration of this operation, to prevent concurrent
 * invocations from corrupting each other's transmissions.  To prevent
 * such interference, only this function should be used to send packets to
 * the client, rather than the lower-level proto_send_packet() function.
 *
 * @param client  The CLIENT who should be sent the packet.
 * @param pkt  The header of the packet to be sent.
 * @param data  Data payload to be sent, or NULL if none.
 * @return 0 if transmission succeeds, -1 otherwise.
 */

int client_send_packet(CLIENT *player, JEUX_PACKET_HEADER *pkt, void *data)
{
    debug("client.c");
    debug("data: %s", (char*)data);

    // Send the packet
    pkt->size = htons(pkt->size);
    int ret = proto_send_packet(player->fd, pkt, data);

    // Release the lock

    return ret;
}

/*
 * Send an ACK packet to a client.  This is a convenience function that
 * streamlines a common case.
 *
 * @param client  The CLIENT who should be sent the packet.
 * @param data  Pointer to the optional data payload for this packet,
 * or NULL if there is to be no payload.
 * @param datalen  Length of the data payload, or 0 if there is none.
 * @return 0 if transmission succeeds, -1 otherwise.
 */
int client_send_ack(CLIENT *client, void *data, size_t datalen)
{
    debug("client.c");

    int res;
    JEUX_PACKET_HEADER pkt = {0};
    pkt.type = JEUX_ACK_PKT;
    pkt.size = datalen;
    res = client_send_packet(client, &pkt, data);
    debug("client send ack return");
    debug("client fd: %d", client->fd);
    debug("%s", (char *)data);
    return res;
}

/*
 * Send an NACK packet to a client.  This is a convenience function that
 * streamlines a common case.
 *
 * @param client  The CLIENT who should be sent the packet.
 * @return 0 if transmission succeeds, -1 otherwise.
 */

int client_send_nack(CLIENT *client)
{
    debug("client.c");

    int res;
    pthread_mutex_lock(&client->lock);

    JEUX_PACKET_HEADER header;
    header.type = JEUX_NACK_PKT;
    header.size = 0;
    res = client_send_packet(client, &header, NULL);
    pthread_mutex_unlock(&client->lock);

    return res;
}

/*
 * Add an INVITATION to the list of outstanding invitations for a
 * specified CLIENT.  A reference to the INVITATION is retained by
 * the CLIENT and the reference count of the INVITATION is
 * incremented.  The invitation is assigned an integer ID,
 * which the client subsequently uses to identify the invitation.
 *
 * @param client  The CLIENT to which the invitation is to be added.
 * @param inv  The INVITATION that is to be added.
 * @return  The ID assigned to the invitation, if the invitation
 * was successfully added, otherwise -1.
 */
int client_add_invitation(CLIENT *client, INVITATION *inv)
{
    debug("client.c");

    debug("123");
    INVITATION_NODE *invitation_node = malloc(sizeof(invitation_node));
    invitation_node->next = NULL;
    invitation_node->invitation = inv;

    if (client->invitations == NULL)
    {
        client->invitations = invitation_node;
        invitation_node->id = 0;
    }
    else
    {
        INVITATION_NODE *iter = client->invitations;
        int id = 0;
        int gap_flag = 0;
        while (iter->next != NULL)
        {
            if (!gap_flag && iter->id != id)
            {
                gap_flag = 1;
            }
            else
            {
                id++;
            }
            iter = iter->next;
        }
        iter->next = invitation_node;
        invitation_node->id = id;
    }
    inv_ref(invitation_node->invitation, "client_add_invitation");

    return 0;
}

/*
 * Remove an invitation from the list of outstanding invitations
 * for a specified CLIENT.  The reference count of the invitation is
 * decremented to account for the discarded reference.
 *
 * @param client  The client from which the invitation is to be removed.
 * @param inv  The invitation that is to be removed.
 * @return the CLIENT's id for the INVITATION, if it was successfully
 * removed, otherwise -1.
 */
int client_remove_invitation(CLIENT *client, INVITATION *inv)
{
    debug("client.c");

    pthread_mutex_lock(&client->lock);

    if (client->invitations->invitation == inv)
    {
        client->invitations = NULL;
        inv_unref(inv, "client remove invitation");
    }

    INVITATION_NODE *inv_node = client->invitations;
    INVITATION_NODE *prev = inv_node;

    while (inv_node != NULL && inv_node->invitation != inv)
    {
        prev = inv_node;
        inv_node = inv_node->next;
    }
    if (inv_node == NULL)
    {
        debug("ERROR!");
        return -1;
    }
    prev->next = inv_node->next;
    inv_unref(inv_node->invitation, "client_remove_invitation");

    pthread_mutex_unlock(&client->lock);

    return 0;
};

/*
 * Make a new invitation from a specified "source" CLIENT to a specified
 * target CLIENT.  The invitation represents an offer to the target to
 * engage in a game with the source.  The invitation is added to both the
 * source's list of invitations and the target's list of invitations and
 * the invitation's reference count is appropriately increased.
 * An `INVITED` packet is sent to the target of the invitation.
 *
 * @param source  The CLIENT that is the source of the INVITATION.
 * @param target  The CLIENT that is the target of the INVITATION.
 * @param source_role  The GAME_ROLE to be played by the source of the INVITATION.
 * @param target_role  The GAME_ROLE to be played by the target of the INVITATION.
 * @return the ID assigned by the source to the INVITATION, if the operation
 * is successful, otherwise -1.
 */
int client_make_invitation(CLIENT *source, CLIENT *target,
                           GAME_ROLE source_role, GAME_ROLE target_role)
{
    debug("client.c");

    pthread_mutex_lock(&source->lock);
    pthread_mutex_lock(&target->lock);

    // Check that both clients are logged in
    if (!source->player || !target->player)
    {
        return -1;
    }

    // Create a new invitation
    INVITATION *invitation = inv_create(source, target,
                                        source_role, target_role);
    if (!invitation)
    {
        return -1;
    }

    // Add the invitation to both clients' lists
    int source_inv_id = client_add_invitation(source, invitation);
    int target_inv_id = client_add_invitation(target, invitation);

    if (target_inv_id == -1)
    {
        client_remove_invitation(source, invitation);
        return -1;
    }
    if (source_inv_id == -1)
    {
        client_remove_invitation(target, invitation);
        return -1;
    }

    // Increase the invitation's reference count
    inv_ref(invitation, "client_make_invitation");

    // Send an INVITED packet to the target

    JEUX_PACKET_HEADER *hdr = malloc(sizeof(JEUX_PACKET_HEADER));

    // TODO: double check invite vs invited
    hdr->type = JEUX_INVITED_PKT;

    int res = client_send_packet(target, hdr, NULL);
    if (res != 0)
    {
        // Failed to send the packet, so we need to clean up
        client_remove_invitation(source, invitation);
        client_remove_invitation(target, invitation);
        inv_unref(invitation, "ERROR");
        return -1;
    }

    // Assign the invitation ID and return it

    int inv_id = source_inv_id;

    pthread_mutex_unlock(&source->lock);
    pthread_mutex_unlock(&target->lock);

    return inv_id;
};

/*
 * Revoke an invitation for which the specified CLIENT is the source.
 * The invitation is removed from the lists of invitations of its source
 * and target CLIENT's and the reference counts are appropriately
 * decreased.  It is an error if the specified CLIENT is not the source
 * of the INVITATION, or the INVITATION does not exist in the source or
 * target CLIENT's list.  It is also an error if the INVITATION being
 * revoked is in a state other than the "open" state.  If the invitation
 * is successfully revoked, then the target is sent a REVOKED packet
 * containing the target's ID of the revoked invitation.
 *
 * @param client  The CLIENT that is the source of the invitation to be
 * revoked.
 * @param id  The ID assigned by the CLIENT to the invitation to be
 * revoked.
 * @return 0 if the invitation is successfully revoked, otherwise -1.
 */

INVITATION *client_find_invitation(CLIENT *client, int id)
{
    debug("client.c");

    INVITATION_NODE *inv_node;
    INVITATION *inv;
    inv_node = client->invitations;
    while (inv_node != NULL)
    {
        debug("inside the while loop");
        inv = inv_node->invitation;
        if (inv_node->id == id)
        {
            debug("exit");
            return inv;
        }
        inv_node = inv_node->next;
    }

    debug("exit");
    return NULL;
}

int client_get_inv_id(CLIENT *client, INVITATION *invitation)
{
    debug("client.c");

    INVITATION_NODE *inv_node;
    INVITATION *inv;
    inv_node = client->invitations;
    while (inv_node != NULL)
    {
        inv = inv_node->invitation;
        if (inv == invitation)
        {
            pthread_mutex_unlock(&(inv->mutex)); // Unlock the invitation
            return inv_node->id;
        }
        inv_node = inv_node->next;
    }
    return -1;
}

int client_revoke_invitation(CLIENT *client, int id)
{
    debug("client.c");

    // Lock the mutex to ensure thread safety
    pthread_mutex_lock(&client->lock);

    // Look for the invitation in the source client's list of invitations
    INVITATION *invitation = client_find_invitation(client, id);
    if (invitation == NULL)
    {
        pthread_mutex_unlock(&client->lock);
        return -1; // Invitation not found
    }

    if (inv_get_source(invitation) != client)
    {
        pthread_mutex_unlock(&client->lock);
        return -1;
    }

    // Make sure the invitation is in the "open" state
    if (inv_get_game(invitation) != NULL)
    {
        pthread_mutex_unlock(&client->lock);
        return -1; // Invitation not in "open" state
    }

    // Remove the invitation from the source client's list of invitations
    client_remove_invitation(client, invitation);

    inv_unref(invitation, "client_revoke_invitation");

    JEUX_PACKET_HEADER *hdr = {0};
    hdr->type = JEUX_REVOKE_PKT;
    hdr->id = client_remove_invitation(
        invitation->target,
        invitation);

    if (client_send_packet(invitation->target, hdr, NULL))
    {
        return -1;
    }

    // Unlock the mutex
    pthread_mutex_unlock(&client->lock);

    return 0; // Successfully revoked invitation
}

/*
 * Decline an invitation previously made with the specified CLIENT as target.
 * The invitation is removed from the lists of invitations of its source
 * and target CLIENT's and the reference counts are appropriately
 * decreased.  It is an error if the specified CLIENT is not the target
 * of the INVITATION, or the INVITATION does not exist in the source or
 * target CLIENT's list.  It is also an error if the INVITATION being
 * declined is in a state other than the "open" state.  If the invitation
 * is successfully declined, then the source is sent a DECLINED packet
 * containing the source's ID of the declined invitation.
 *
 * @param client  The CLIENT that is the target of the invitation to be
 * declined.
 * @param id  The ID assigned by the CLIENT to the invitation to be
 * declined.
 * @return 0 if the invitation is successfully declined, otherwise -1.
 */

int client_decline_invitation(CLIENT *client, int id)
{
    debug("client.c");

    // Lock the mutex to ensure thread safety
    pthread_mutex_lock(&client->lock);

    // Look for the invitation in the source client's list of invitations
    INVITATION *invitation = client_find_invitation(client, id);
    if (invitation == NULL)
    {
        pthread_mutex_unlock(&client->lock);
        return -1; // Invitation not found
    }

    if (inv_get_target(invitation) != client)
    {
        pthread_mutex_unlock(&client->lock);
        return -1;
    }

    // Make sure the invitation is in the "open" state
    if (inv_get_game(invitation) != NULL)
    {
        pthread_mutex_unlock(&client->lock);
        return -1; // Invitation not in "open" state
    }

    // Remove the invitation from the source client's list of invitations
    client_remove_invitation(client, invitation);

    inv_unref(invitation, "client_decline_invitation");

    JEUX_PACKET_HEADER *hdr = {0};
    hdr->type = JEUX_REVOKE_PKT;
    hdr->id = client_remove_invitation(
        invitation->source,
        invitation);

    if (client_send_packet(invitation->source, hdr, NULL))
    {
        return -1;
    }

    // Unlock the mutex
    pthread_mutex_unlock(&client->lock);

    return 0; // Successfully revoked invitation
}

/*
 * Accept an INVITATION previously made with the specified CLIENT as
 * the target.  A new GAME is created and a reference to it is saved
 * in the INVITATION.  If the invitation is successfully accepted,
 * the source is sent an ACCEPTED packet containing the source's ID
 * of the accepted INVITATION.  If the source is to play the role of
 * the first player, then the payload of the ACCEPTED packet contains
 * a string describing the initial game state.  A reference to the
 * new GAME (with its reference count incremented) is returned to the
 * caller.
 *
 * @param client  The CLIENT that is the target of the INVITATION to be
 * accepted.
 * @param id  The ID assigned by the target to the INVITATION.
 * @param strp  Pointer to a variable into which will be stored either
 * NULL, if the accepting client is not the first player to move,
 * or a malloc'ed string that describes the initial game state,
 * if the accepting client is the first player to move.
 * If non-NULL, this string should be used as the payload of the `ACK`
 * message to be sent to the accepting client.  The caller must free
 * the string after use.
 * @return 0 if the INVITATION is successfully accepted, otherwise -1.
 */

int client_accept_invitation(CLIENT *client, int id, char **strp)
{
    debug("client.c");

    pthread_mutex_lock(&client->lock);
    INVITATION *inv = client_find_invitation(client, id);

    if (inv == NULL || inv_get_game(inv) != NULL)
    {
        pthread_mutex_unlock(&client->lock);
        return -1; // Invitation not found or not in open state
    }

    debug("here");

    if (inv_accept(inv) == -1)
    {
        return -1;
    }
    debug("here");

    // Send the ACCEPTED packet to the source client
    JEUX_PACKET_HEADER hdr = {0};
    hdr.type = JEUX_ACCEPTED_PKT;
    debug("here");

    char *unparse_state = game_unparse_state(inv_get_game(inv));

    // invite b 2
    if (inv_get_source_role(inv) == FIRST_PLAYER_ROLE)
    {
        hdr.size = strlen(unparse_state) + 1;


        debug("here");

        hdr.id = client_get_inv_id(inv_get_source(inv), inv);
        debug("here");
        if (client_send_packet(inv_get_source(inv), &hdr, unparse_state) == -1)
        {
            return -1;
        }
        *strp = NULL;

    }
    // invite b 1
    else
    {
        debug("enter else");

        *strp = malloc(strlen(unparse_state) + 1);
        strcpy(*strp, unparse_state);

        hdr.size = 0;
        client_send_packet(inv_get_source(inv), &hdr, NULL);
    }

    debug("here");

    pthread_mutex_unlock(&client->lock);
    return 0;
};

/*
 * Resign a game in progress.  This function may be called by a CLIENT
 * that is either source or the target of the INVITATION containing the
 * GAME that is to be resigned.  It is an error if the INVITATION containing
 * the GAME is not in the ACCEPTED state.  If the game is successfully
 * resigned, the INVITATION is set to the CLOSED state, it is removed
 * from the lists of both the source and target, and a RESIGNED packet
 * containing the opponent's ID for the INVITATION is sent to the opponent
 * of the CLIENT that has resigned.
 *
 * @param client  The CLIENT that is resigning.
 * @param id  The ID assigned by the CLIENT to the INVITATION that contains
 * the GAME to be resigned.
 * @return 0 if the game is successfully resigned, otherwise -1.
 */

int client_resign_game(CLIENT *client, int id)
{
    debug("client.c");

    pthread_mutex_lock(&client->lock);
    INVITATION *inv = client_find_invitation(client, id);
    pthread_mutex_unlock(&client->lock);

    pthread_mutex_lock(&inv->source->lock);
    pthread_mutex_lock(&inv->target->lock);

    CLIENT *opponent = inv->source == client ? inv->target : inv->source;

    if (inv == NULL)
    {
        return -1; // Invitation not found
    }

    // Check if the invitation is in the ACCEPTED state
    if (inv_get_game(inv) == NULL)
    {
        return -1; // Invitation not in ACCEPTED state
    }

    // Get the opponent's fd
    // int opponent_fd = (inv_get_source(inv) == client) ? client_get_fd(inv_get_target(inv))
    //                                                   : client_get_fd(inv_get_source(inv));

    // Remove the invitation from the lists of both the source and target clients

    client_remove_invitation(inv->source, inv);

    client_remove_invitation(inv->target, inv);
    pthread_mutex_unlock(&inv->target->lock);

    // Send a RESIGNED packet containing the opponent's ID to the opponent

    JEUX_PACKET_HEADER hdr = {0};
    hdr.id = client_get_inv_id(client, inv);
    hdr.type = JEUX_RESIGNED_PKT;
    hdr.size = 0;

    client_send_packet(opponent, &hdr, NULL);

    // Decrement the reference count of the invitation

    inv_unref(inv, "client_resign_game");

    // If the game has no other references, free its memory
    if (inv->game->refcount == 0)
    {
        free(inv->game);
    }
    pthread_mutex_unlock(&inv->source->lock);
    pthread_mutex_unlock(&inv->target->lock);

    return 0;
}

/*
 * Make a move in a game currently in progress, in which the specified
 * CLIENT is a participant.  The GAME in which the move is to be made is
 * specified by passing the ID assigned by the CLIENT to the INVITATION
 * that contains the game.  The move to be made is specified as a string
 * that describes the move in a game-dependent format.  It is an error
 * if the ID does not refer to an INVITATION containing a GAME in progress,
 * if the move cannot be parsed, or if the move is not legal in the current
 * GAME state.  If the move is successfully made, then a MOVED packet is
 * sent to the opponent of the CLIENT making the move.  In addition, if
 * the move that has been made results in the game being over, then an
 * ENDED packet containing the appropriate game ID and the game result
 * is sent to each of the players participating in the game, and the
 * INVITATION containing the now-terminated game is removed from the lists
 * of both the source and target.  The result of the game is posted in
 * order to update both players' ratings.
 *
 * @param client  The CLIENT that is making the move.
 * @param id  The ID assigned by the CLIENT to the GAME in which the move
 * is to be made.
 * @param move  A string that describes the move to be made.
 * @return 0 if the move was made successfully, -1 otherwise.
 */

int client_make_move(CLIENT *client, int id, char *move)
{
    debug("client.c");
    pthread_mutex_lock(&client->lock);

    debug("client_make_move");

    INVITATION *inv = client_find_invitation(client, id);
    if (!inv || inv_get_game(inv) == NULL)
    {
        debug("exit");

        return -1; // invalid game ID or game not in progress
    }

    debug("client_make_move");

    GAME *game = inv_get_game(inv);

    GAME_ROLE role;
    CLIENT *opponent;

    debug("client_make_move");

    if (inv_get_source(inv) == client)
    {
        role = inv_get_source_role(inv);
        opponent = inv_get_target(inv);
    }
    else
    {
        role = inv_get_target_role(inv);
        opponent = inv_get_source(inv);
    }

    debug("client_make_move");


    GAME_MOVE *game_move = game_parse_move(game, role, move);
    if(game_apply_move(game, game_move)){
        return -1;
    }

    if (game_move == NULL)
    {
        return -1; // move parsing failed
    }

    debug("client_make_move");


    JEUX_PACKET_HEADER header = {0};

    header.type = JEUX_MOVED_PKT;
    char* unparse_state = game_unparse_state(game);
    header.size = strlen(unparse_state);

    debug("unparse_state: %s", unparse_state);
    client_send_packet(opponent, &header, unparse_state);

    debug("client_make_move");

    if (game_is_over(game))
    {
        debug("client_make_move");
        

        /*In addition, if
         * the move that has been made results in the game being over, then an
         * ENDED packet containing the appropriate game ID and the game result
         * is sent to each of the players participating in the game, and the
         * INVITATION containing the now-terminated game is removed from the lists
         * of both the source and target.
         * */
        JEUX_PACKET_HEADER header2 = {0};

        header2.type = JEUX_ENDED_PKT;
        header2.id = client_get_inv_id(client, inv);
        header2.role = game_get_winner(game);
        header2.size = 0;
        

        client_send_packet(inv->source, &header2, NULL);
        client_send_packet(inv->target, &header2, NULL);
        debug("client_make_move");


        client_remove_invitation(inv->source, inv);
        client_remove_invitation(inv->target, inv);

        player_post_result(client_get_player(inv_get_source(inv)),
        client_get_player(inv_get_target(inv)), 
        game_get_winner(game));

    debug("client_make_move");

    }

    free(game_move);
    pthread_mutex_unlock(&client->lock);
    debug("client_make_move exit");



    return 0; // success
}
