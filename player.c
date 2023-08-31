#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <pthread.h>
#include "player.h"
#include "global.h"
#include "debug.h"

/* Function prototypes */
void update_rating(PLAYER *player, double score, double expected_score);

/*
 * made of the username that is passed.  The newly created PLAYER has
 * a reference count of one, corresponding to the reference that is
 * returned from this function.
 *
 * @param name  The username of the PLAYER.
 * @return  A reference to the newly created PLAYER, if initialization
 * was successful, otherwise NULL.
 */
PLAYER *player_create(char *name)
{
    debug("%d", __LINE__);

    PLAYER *player = malloc(sizeof(PLAYER));
    if (player == NULL)
    {
        return NULL;
    }
    player->name = strdup(name);
    if (player->name == NULL)
    {
        free(player);
        return NULL;
    }
    player->rating = PLAYER_INITIAL_RATING;
    player->ref_count = 0;
    player_ref(player, "newly created player");
    if (pthread_mutex_init(&player->lock, NULL) != 0)
    {
        free(player->name);
        free(player);
        return NULL;
    }
    return player;
}

/* Increases the reference count of a player */
/*
 * Increase the reference count on a player by one.
 *
 * @param player  The PLAYER whose reference count is to be increased.
 * @param why  A string describing the reason why the reference count is
 * being increased.  This is used for debugging printout, to help trace
 * the reference counting.
 * @return  The same PLAYER object that was passed as a parameter.
 */
PLAYER *player_ref(PLAYER *player, char *why)
{
    debug("%d", __LINE__);

    pthread_mutex_lock(&player->lock);
    player->ref_count++;
    pthread_mutex_unlock(&player->lock);
    return player;
}

/*
 * Decrease the reference count on a PLAYER by one.
 * If after decrementing, the reference count has reached zero, then the
 * PLAYER and its contents are freed.
 *
 * @param player  The PLAYER whose reference count is to be decreased.
 * @param why  A string describing the reason why the reference count is
 * being decreased.  This is used for debugging printout, to help trace
 * the reference counting.
 *
 */
/* Decreases the reference count of a player and frees the object if the count is zero */
void player_unref(PLAYER *player, char *why)
{
    debug("%d", __LINE__);

    pthread_mutex_lock(&player->lock);
    player->ref_count--;
    if (player->ref_count == 0)
    {
        pthread_mutex_unlock(&player->lock);
        pthread_mutex_destroy(&player->lock);
        free(player->name);
        free(player);
    }
    else
    {
        pthread_mutex_unlock(&player->lock);
    }
}

/*
 * Get the username of a player.
 *
 * @param player  The PLAYER that is to be queried.
 * @return the username of the player.
 */
/* Returns the name of a player */
char *player_get_name(PLAYER *player)
{
    debug("%d", __LINE__);

    if (player == NULL){
        debug("player is null");
        return NULL;
    }

    return player->name;
}

/* Returns the rating of a player */
/*
 * Get the rating of a player.
 *
 * @param player  The PLAYER that is to be queried.
 * @return the rating of the player.
 */
int player_get_rating(PLAYER *player)
{
    debug("%d", __LINE__);

    return player->rating;
}

/* Posts the result of a game between two players */
/*
 * Post the result of a game between two players.
 * To update ratings, we use a system of a type devised by Arpad Elo,
 * similar to that used by the US Chess Federation.
 * The player's ratings are updated as follows:
 * Assign each player a score of 0, 0.5, or 1, according to whether that
 * player lost, drew, or won the game.
 * Let S1 and S2 be the scores achieved by player1 and player2, respectively.
 * Let R1 and R2 be the current ratings of player1 and player2, respectively.
 * Let E1 = 1/(1 + 10**((R2-R1)/400)), and
 *     E2 = 1/(1 + 10**((R1-R2)/400))
 * Update the players ratings to R1' and R2' using the formula:
 *     R1' = R1 + 32*(S1-E1)
 *     R2' = R2 + 32*(S2-E2)
 *
 * @param player1  One of the PLAYERs that is to be updated.
 * @param player2  The other PLAYER that is to be updated.
 * @param result   0 if draw, 1 if player1 won, 2 if player2 won.
 */
void player_post_result(PLAYER *player1, PLAYER *player2, int result)
{
    debug("%d", __LINE__);

    double score1, score2, E1, E2;
    int R1 = player1->rating;
    int R2 = player2->rating;
    E1 = 1.0 / (1.0 + pow(10.0, (double)(R2 - R1) / 400.0));
    E2 = 1.0 / (1.0 + pow(10.0, (double)(R1 - R2) / 400.0));
    if (result == 0)
    {
        score1 = 0.5;
        score2 = 0.5;
    }
    else if (result == 1)
    {
        score1 = 1.0;
        score2 = 0.0;
    }
    else if (result == 2)
    {
        score1 = 0.0;
        score2 = 1.0;
    }

    update_rating(player1, score1, E1);
    update_rating(player2, score2, E2);
}

/* Updates the rating of a player
 * Update the players ratings to R1' and R2' using the formula:
 *     R1' = R1 + 32*(S1-E1)
 *     R2' = R2 + 32*(S2-E2)
 */
void update_rating(PLAYER *player, double score, double expected_score)
{
    debug("%d", __LINE__);

    pthread_mutex_lock(&player->lock);
    player->rating += (int)round(32.0 * (score - expected_score));
    pthread_mutex_unlock(&player->lock);
}
