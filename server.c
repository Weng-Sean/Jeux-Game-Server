#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include "client_registry.h"
#include "jeux_globals.h"
#include "debug.h"
// #include "server.h"
// #include "protocol.h"
#include "player_registry.h"
// #include "game.h"
#include "global.h"
#include "string.h"



/*
 * Thread function for the thread that handles a particular client.
 *
 * @param  Pointer to a variable that holds the file descriptor for
 * the client connection.  This pointer must be freed once the file
 * descriptor has been retrieved.
 * @return  NULL
 *
 * This function executes a "service loop" that receives packets from
 * the client and dispatches to appropriate functions to carry out
 * the client's requests.  It also maintains information about whether
 * the client has logged in or not.  Until the client has logged in,
 * only LOGIN packets will be honored.  Once a client has logged in,
 * LOGIN packets will no longer be honored, but other packets will be.
 * The service loop ends when the network connection shuts down and
 * EOF is seen.  This could occur either as a result of the client
 * explicitly closing the connection, a timeout in the network causing
 * the connection to be closed, or the main thread of the server shutting
 * down the connection as part of graceful termination.
 */



void *jeux_client_service(void *arg) {
    int fd = *(int *)arg;
    free(arg);

    // detach thread
    pthread_detach(pthread_self());

    // CLIENT_REGISTRY *client_registry = creg_init();

    // // register client file descriptor with the client registry
    CLIENT *client = creg_register(client_registry, fd);

    // service loop
    int logged_in = 0;
    // int source_id = -1;

    // int invitation_ID = 0;

    // PLAYER *player = NULL;
    // GAME *game = NULL;
    while (1) {
        JEUX_PACKET_HEADER* hdr = malloc(sizeof(JEUX_PACKET_HEADER));
        void *payload;
        if (proto_recv_packet(fd, hdr, &payload)) {
            free(hdr);
            break;
        }
            debug("payload: %s", (char*)payload);

        switch (hdr->type) {
            /*
            LOGIN:  The payload portion of the packet contains the player username
            (not null-terminated) given by the user.
            Upon receipt of a LOGIN packet, the client_login() function should be called.
            In case of a successful LOGIN an ACK packet with no payload should be
            sent back to the client.  In case of an unsuccessful LOGIN, a NACK packet
            (also with no payload) should be sent back to the client.

            Until a LOGIN has been successfully processed, other packets sent by the
            client should elicit a NACK response from the server.
            Once a LOGIN has been successfully processed, other packets should be
            processed normally, and LOGIN packets should result in a NACK.
            */

            case JEUX_LOGIN_PKT:
                debug("packet");
                if (logged_in) {
                    // JEUX_PACKET_HEADER *header = malloc(sizeof(JEUX_PACKET_HEADER));
                    // header->type = JEUX_NACK_PKT;
                    // header->size = 0;
                    // proto_send_packet(fd, header, NULL);

                    client_send_nack(client);
                    
                    break;
                }
                
                logged_in = 1;

                
                // JEUX_PACKET_HEADER *header = malloc(sizeof(JEUX_PACKET_HEADER));
                // header->type = JEUX_ACK_PKT;
                // header->size = 0;
                // proto_send_packet(fd, header, NULL);
                PLAYER *player = preg_register(player_registry, (char*)payload);
                client_login(client, player);

                client_send_ack(client, NULL, 0);
                break;


            /*
            USERS:  This type of packet has no payload.  The server responds by
            sending an ACK packet whose payload consists of a text string in which
            each line gives the username of a currently logged in player, followed by
            a single TAB character, followed by the player's current rating.
            */

            case JEUX_USERS_PKT:
                debug("packet");

                if (!logged_in) {
                    client_send_nack(client);
                    break;
                }
                
                // Create an empty response string
                char response_str[9000] = {0};

                // Loop through all logged-in players and append their usernames and ratings
                PLAYER **players = creg_all_players(client_registry);


                for (int i = 0; players[i] != NULL; i++) {
                    debug("in the loop");
                    // Get the player's username and rating
                    const char *username = player_get_name(players[i]);
                    debug("%s", username);
                    int rating = player_get_rating(players[i]);

                    // Append the username and rating to the response string
                    snprintf(response_str + strlen(response_str), sizeof(response_str) - strlen(response_str), "%s\t%d\n", username, rating);
                }

                // Send an ACK packet with the response string as the payload
                // JEUX_PACKET_HEADER *header = malloc(sizeof(JEUX_PACKET_HEADER));
                // header->type = JEUX_ACK_PKT;
                // proto_send_packet(fd, header, response_str);
                debug("%s", response_str);
                
                client_send_ack(client, response_str, strlen(response_str));

                free(players);
                break;

            /*
            INVITE:  The payload of this type of packet is the username of another
            player, who is invited to play a game.  The sender of the INVITE is the
            "source" of the invitation; the invited player is the "target".
            The role field of the header contains an integer value that specifies the
            role in the game to which the player is invited (1 for first player to move,
            2 for second player to move).
            The server responds either by sending an ACK with no payload in case of
            success or a NACK with no payload in case of error.  In case of an ACK,
            the id field of the ACK packet will contain the integer ID that the
            source client can use to identify that invitation in the future.
            An INVITED packet will be sent to the target as a notification that the
            invitation has been made.  This id field of this packet gives an ID that
            the target can use to identify the invitation.  Note that, in general,
            the IDs used by the source and target to refer to an invitation will be
            different from each other.
            */

            case JEUX_INVITE_PKT:
                debug("packet");

                if (!logged_in) {
                    client_send_nack(client);
                    break;
                }
                
                int role = hdr->role;

                if (role != 1 && role != 2) {
                    client_send_nack(client);
                    break;
                }

                CLIENT* target = creg_lookup(client_registry, (char*)payload);

                client_make_invitation(
                    client, target, 
                role == 1? SECOND_PLAYER_ROLE : FIRST_PLAYER_ROLE,
                role == 1? FIRST_PLAYER_ROLE : SECOND_PLAYER_ROLE
                );

                
                client_send_ack(client, NULL, 0);
            
                break;


            /*
            REVOKE:  This type of packet has no payload.  The id field of the header
            contains the ID of the invitation to be revoked.  The revoking player must
            be the source of that invitation.  The server responds by
            attempting to revoke the invitation.  If successful, an ACK with no payload
            sent, otherwise a NACK with no payload is sent.  A successful revocation causes
            a REVOKED packet to be sent to notify the invitation target.
            */
            case JEUX_REVOKE_PKT:
                debug("packet");

                if (!logged_in) {
                    client_send_nack(client);
                    break;
                }
                client_revoke_invitation(client, hdr->id);
                client_send_ack(client, NULL, 0);

               
                break;

            /*
              This type of packet is similar to REVOKE, except that it is
                sent by the target of an invitation in order to decline it.  The server's
                response is either an ACK or NACK as for REVOKE.  If the invitation is
                successfully declined, a DECLINED packet is sent to notify the source.
            */
            case JEUX_DECLINE_PKT:
                debug("packet");

                if (!logged_in) {
                    client_send_nack(client);
                    break;
                }
                if (client_decline_invitation(client, hdr->id)){
                    client_send_nack(client);
                    break;
                }
                client_send_ack(client, NULL, 0);
               
                break;
            
            /*
              This type of packet is sent by the target of an invitation in
                order to accept it.  The id field of the header contains the ID of the invitation
                to be accepted.  If the invitation has been revoked or previously accepted,
                a NACK is sent by the server.  Otherwise a new game is created and an ACK
                is sent by the server.  If the target's role in the game is that of first player
                to move, then the payload of the ACK will contain a string describing the
                initial game state.  In addition, the source of the invitation will be sent an
                ACCEPTED packet, the id field of which contains the source's ID for the
                invitation.  If the source's role is that of the first player to move, then
                the payload of the ACCEPTED packet will contain a string describing the
                initial game state.
            */
            case JEUX_ACCEPT_PKT:
                debug("packet");
                char* msg = NULL;

                if (!logged_in) {
                    client_send_nack(client);
                    break;
                }
                if (client_accept_invitation(client, hdr->id, &msg)){
                    client_send_nack(client);
                    break;
                }
                if (msg != NULL){
                    client_send_ack(client, msg, strlen(msg));
                    free(msg);
                }
                else{
                    client_send_ack(client, NULL, 0);
                }
               
                break;
            
            /*
            This type of packet is sent by a client to make a move in a game
            in progress.  The id field of the header contains the client's ID for the
            invitation that resulted in the game.  The payload of the packet contains a
            string describing the move.  For the tic-tac-toe game, a move string may
            consist either of a single digit in the range ['1' - '9'], or a string consisting
            of such a digit, followed either by "<-X" or "<-O".  The latter forms specify
            the role of the player making the move as well as the square to be occupied
            by the player's mark.  The server will respond with ACK with no payload if
            the move is legal and is successfully applied to the game state, otherwise
            with NACK with no payload.  In addition, the opponent of the player making
            the move will be sent a MOVED packet, the id field of which contains the
            opponent's ID for the game and the payload of which contains a string that
            describes the new game state after the move.
            */
            case JEUX_MOVE_PKT:
                debug("packet");

                if (!logged_in) {
                    client_send_nack(client);
                    break;
                }
                if (client_make_move(client, hdr->id, payload)){
                    client_send_nack(client);
                    break;
                }
                client_send_ack(client, NULL, 0);

                break;


            /*
            This type of packet is sent by a client to resign a game in
            progress.  The id field of the header contains the client's ID for the
            invitation that resulted in the game.  There is no payload.
            If the resignation is successful, then the server responds with ACK,
            otherwise with NACK.  In addition, the opponent of the player who is
            resigning is sent a RESIGNED packet, the id field of which contains the
            opponent's ID for the game.
            */
            case JEUX_RESIGN_PKT:
                debug("packet");

                if (!logged_in) {
                    client_send_nack(client);
                    break;
                }
                if (client_resign_game(client, hdr->id)){
                    client_send_nack(client);
                    break;
                }
                client_send_ack(client, NULL, 0);
               
                break;
            
            case JEUX_ENDED_PKT:
                break;
            default:
                client_send_nack(client);
                break;


        }
        free(payload);
        free(hdr);

    }

    if (client_get_player(client) != NULL){
        client_logout(client);
    }
    creg_unregister(client_registry, client);

    return NULL;
}

