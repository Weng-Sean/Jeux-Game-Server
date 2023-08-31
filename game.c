#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "game.h"
#include "global.h"
#include <pthread.h>
#include "debug.h"

#define MAX_MOVE_STRING_LENGTH 256



/**
 * Check if the game is over and update the game's state accordingly.
 *
 * @param game The GAME to be checked and updated.
 * @return 0 if the game is not over or 1 if Player 1 won or 2 if Player 2 won or 3 if tie
 */
static int check_game_over(GAME *game)
{
    debug("%d", __LINE__);

    // Check if either player has resigned
    if (game->first_player_resigned || game->second_player_resigned)
    {
        game->game_over = 1;
        return 1;
    }

    // Check if the game has reached a win or draw state
    int res = 0;

    for (int i = 0; i < 3; i++)
    {
        if (game->game_board[i][0] + game->game_board[i][1] + game->game_board[i][2] == 3)
        {
            res = 1;
            break;
        }
        if (game->game_board[0][i] + game->game_board[1][i] + game->game_board[2][i] == 3)
        {
            res = 1;
            break;
        }
        if (game->game_board[i][0] + game->game_board[i][1] + game->game_board[i][2] == -3)
        {
            res = 2;
            break;
        }
        if (game->game_board[0][i] + game->game_board[1][i] + game->game_board[2][i] == -3)
        {
            res = 2;
            break;
        }
    }

    if (res == 0)
    {

        if (game->game_board[0][0] + game->game_board[1][1] + game->game_board[2][2] == 3)
        {
            res = 1;
        }
        else if (game->game_board[0][2] + game->game_board[1][1] + game->game_board[2][0] == 3)
        {
            res = 1;
        }
        else if (game->game_board[0][0] + game->game_board[1][1] + game->game_board[2][2] == -3)
        {
            res = 2;
        }
        else if (game->game_board[0][2] + game->game_board[1][1] + game->game_board[2][0] == -3)
        {
            res = 2;
        }
    }

    if (res == 0)
    {
        int found_zero = 0;
        for (int i = 0; i < 3; i++)
        {
            for (int j = 0; j < 3; j++)
            {
                if (game->game_board[i][j] == 0)
                {
                    found_zero = 1;
                }
            }
        }

        if (!found_zero)
        {
            res = 3;
        }
    }

    if (res != 0)
    {
        game->game_over = 1;
        return res;
    }

    return 0;
}

/**
 * Create a new game in an initial state.  The returned game has a
 * reference count of one.
 *
 * @return the newly created GAME, if initialization was successful,
 * otherwise NULL.
 */
GAME *game_create(void)
{
    debug("%d", __LINE__);

    GAME *game = malloc(sizeof(GAME));
    if (game == NULL)
    {
        return NULL;
    }

    pthread_mutex_init(&(game->mutex), NULL);
    game->current_role = FIRST_PLAYER_ROLE;
    game->game_over = 0;
    game->first_player_resigned = 0;
    game->second_player_resigned = 0;
    game->last_move = NULL;
    game->refcount = 1;
    game->id = 0;

    // initialize game board to all zeroes
    memset(game->game_board, 0, sizeof(game->game_board));

    return game;
}

/**
 * Increase the reference count on a game by one.
 *
 * @param game  The GAME whose reference count is to be increased.
 * @param why  A string describing the reason why the reference count is
 * being increased.  This is used for debugging printout, to help trace
 * the reference counting.
 * @return  The same GAME object that was passed as a parameter.
 */
GAME *game_ref(GAME *game, char *why)
{
    debug("%d", __LINE__);
    
    game->refcount++;
    return game;
}

/**
 * Decrease the reference count on a game by one.  If after
 * decrementing, the reference count has reached zero, then the
 * GAME and its contents are freed.
 *
 * @param game  The GAME whose reference count is to be decreased.
 * @param why  A string describing the reason why the reference count is
 * being decreased.  This is used for debugging printout, to help trace
 * the reference counting.
 */
void game_unref(GAME *game, char *why)
{
    debug("%d", __LINE__);

    pthread_mutex_lock(&game->mutex);

    if (game == NULL || game->refcount <= 0)
    {
        pthread_mutex_unlock(&game->mutex);
        return;
    }

    game->refcount--;

    if (game->refcount == 0)
    {
        pthread_mutex_unlock(&game->mutex);
        pthread_mutex_destroy(&game->mutex);
        free(game->last_move);
        free(game);
        return;
    }

    pthread_mutex_unlock(&game->mutex);
}

/**
 * Apply a GAME_MOVE to a GAME.
 * If the move is illegal in the current GAME state, then it is an error.
 *
 * @param game  The GAME to which the move is to be applied.
 * @param move  The GAME_MOVE to be applied to the game.
 * @return 0 if application of the move was successful, otherwise -1.
 */
int game_apply_move(GAME *game, GAME_MOVE *move)
{
    debug("%d", __LINE__);

    if (game->game_over)
    {
        return -1; // cannot apply moves to a finished game
    }
    if (move->value < 1 || move->value > 9)
    {
        return -1; // invalid move value
    }
    if (game->last_move != NULL && game->last_move->value == move->value)
    {
        return -1; // cannot play the same move twice in a row
    }
    

    pthread_mutex_lock(&game->mutex);

    int r = (move->value - 1) / 3;
    int c = (move->value - 1) % 3;

    // apply the move
    if (game->current_role == FIRST_PLAYER_ROLE)
    {
        if (game->first_player_resigned)
        {
            pthread_mutex_unlock(&game->mutex);
            return -1; // game over due to resignation
        }
        game->last_move = move;
        game->current_role = SECOND_PLAYER_ROLE;
        game->game_board[r][c] = 1;
    }
    else if (game->current_role == SECOND_PLAYER_ROLE)
    {
        if (game->second_player_resigned)
        {
            pthread_mutex_unlock(&game->mutex);
            return -1; // game over due to resignation
        }
        game->last_move = move;
        game->current_role = FIRST_PLAYER_ROLE;
        game->game_board[r][c] = -1;
    }

    check_game_over(game);

    pthread_mutex_unlock(&game->mutex);

    return 0;
}

/**

Determine whether the game is over.
@param game The GAME to be checked.
@return true if the game is over, false otherwise.
*/
int game_is_over(GAME *game)
{
    debug("%d", __LINE__);

    pthread_mutex_lock(&(game->mutex));
    int game_over = game->game_over;
    pthread_mutex_unlock(&(game->mutex));

    return game_over;
}
/*
 * Get the GAME_ROLE of the player who has won the game.
 *
 * @param game  The GAME for which the winner is to be obtained.
 * @return  The GAME_ROLE of the winning player, if there is one.
 * If the game is not over, or there is no winner because the game
 * is drawn, then NULL_PLAYER is returned.
 */
GAME_ROLE game_get_winner(GAME *game)
{
    debug("%d", __LINE__);

    if (game->first_player_resigned){
        return SECOND_PLAYER_ROLE;
    }
    if (game->second_player_resigned){
        return FIRST_PLAYER_ROLE;
    }

    if (!game->game_over)
    {
        return NULL_ROLE;
    }

    int game_res = check_game_over(game);

    if (game->first_player_resigned)
    {
        return SECOND_PLAYER_ROLE;
    }
    else if (game->second_player_resigned)
    {
        return FIRST_PLAYER_ROLE;
    }
    else if (game_res == 1)
    {
        return FIRST_PLAYER_ROLE;
    }
    else if (game_res == 2)
    {
        return SECOND_PLAYER_ROLE;
    }
    else if (game_res == 3)
    {
        return NULL_ROLE;
    }

    debug("ERROR: %d", __LINE__);
    return NULL_ROLE;
}

/**

Free all memory associated with a GAME.
@param game The GAME to be freed.
*/
void game_free(GAME *game)
{
    debug("%d", __LINE__);

    free(game->last_move);
    free(game);
}

/*
 * Submit the resignation of the GAME by the player in a specified
 * GAME_ROLE.  It is an error if the game has already terminated.
 *
 * @param game  The GAME to be resigned.
 * @param role  The GAME_ROLE of the player making the resignation.
 * @return 0 if resignation was successful, otherwise -1.
 */
int game_resign(GAME *game, GAME_ROLE role)
{
    debug("%d", __LINE__);

    pthread_mutex_lock(&game->mutex);

    // Check if the game has already terminated
    if (game->game_over)
    {
        pthread_mutex_unlock(&game->mutex);
        return -1;
    }

    // Update the appropriate resigned flag
    if (role == FIRST_PLAYER_ROLE)
    {
        game->first_player_resigned = 1;
    }
    else if (role == SECOND_PLAYER_ROLE)
    {
        game->second_player_resigned = 1;
    }
    else
    {
        // Invalid role
        debug("INVALID ROLE");
        pthread_mutex_unlock(&game->mutex);
        return -1;
    }

    // Set game_over flag if either players have resigned
    if (game->first_player_resigned || game->second_player_resigned)
    {
        game->game_over = 1;
        debug("GAME OVER!");
    }

    pthread_mutex_unlock(&game->mutex);
    return 0;
}

/*
 * Get a string that describes the current GAME state, in a format
 * appropriate for human users.  The returned string is in malloc'ed
 * storage, which the caller is responsible for freeing when the string
 * is no longer required.
 *
 * @param game  The GAME for which the state description is to be
 * obtained.
 * @return  A string that describes the current GAME state.
 */
char *game_unparse_state(GAME *game)
{
    debug("%d", __LINE__);

    char *state_str = malloc(sizeof(char) * 5000);
    if (state_str == NULL)
    {
        return NULL;
    }

    pthread_mutex_lock(&(game->mutex));
    if (game->game_over)
    {

        sprintf(state_str, "Game  #%d is over\n", game->id);
        switch (game_get_winner(game))
        {
        case FIRST_PLAYER_ROLE:
            sprintf(state_str + strlen(state_str), "Player 1 has won");
            break;
        case SECOND_PLAYER_ROLE:
            sprintf(state_str + strlen(state_str), "Player 2 has won");
            break;
        default:
            sprintf(state_str + strlen(state_str), "The game was drawn");
            break;
        }
    }
    else
    {
        sprintf(state_str, "%c|%c|%c\n",
                game->game_board[0][0] == 1 ? 'X' : (game->game_board[0][0] == -1 ? 'O' : ' '),
                game->game_board[0][1] == 1 ? 'X' : (game->game_board[0][1] == -1 ? 'O' : ' '),
                game->game_board[0][2] == 1 ? 'X' : (game->game_board[0][2] == -1 ? 'O' : ' '));
        sprintf(state_str + strlen(state_str), "-----\n");
        sprintf(state_str + strlen(state_str), "%c|%c|%c\n",
                game->game_board[1][0] == 1 ? 'X' : (game->game_board[1][0] == -1 ? 'O' : ' '),
                game->game_board[1][1] == 1 ? 'X' : (game->game_board[1][1] == -1 ? 'O' : ' '),
                game->game_board[1][2] == 1 ? 'X' : (game->game_board[1][2] == -1 ? 'O' : ' '));
        sprintf(state_str + strlen(state_str), "-----\n");
        sprintf(state_str + strlen(state_str), "%c|%c|%c\n",
                game->game_board[2][0] == 1 ? 'X' : (game->game_board[2][0] == -1 ? 'O' : ' '),
                game->game_board[2][1] == 1 ? 'X' : (game->game_board[2][1] == -1 ? 'O' : ' '),
                game->game_board[2][2] == 1 ? 'X' : (game->game_board[2][2] == -1 ? 'O' : ' '));

        sprintf(state_str + strlen(state_str), "%s to move", game->current_role == FIRST_PLAYER_ROLE ? "X" : "O");
    }
    pthread_mutex_unlock(&(game->mutex));

    return state_str;
}

/*
 * Attempt to interpret a string as a move in the specified GAME.
 * If successful, a GAME_MOVE object representing the move is returned,
 * otherwise NULL is returned.  The caller is responsible for freeing
 * the returned GAME_MOVE when it is no longer needed.
 * Refer to the assignment handout for the syntax that should be used
 * to specify a move.
 *
 * @param game  The GAME for which the move is to be parsed.
 * @param role  The GAME_ROLE of the player making the move.
 * If this is not NULL_ROLE, then it must agree with the role that is
 * currently on the move in the game.
 * @param str  The string that is to be interpreted as a move.
 * @return  A GAME_MOVE described by the given string, if the string can
 * in fact be interpreted as a move, otherwise NULL.
 */
GAME_MOVE *game_parse_move(GAME *game, GAME_ROLE role, char *str)
{
    debug("%d", __LINE__);

    GAME_MOVE *move = malloc(sizeof(GAME_MOVE));
    move->value = atoi(str);
    move->role = role;

    return move;

    // if (move->value == 0 || role == NULL_ROLE)
    // {
    //     free(move);
    //     return NULL;
    // }
    // else
    // {
    //     int r = (move->value - 1) / 3;
    //     int c = (move->value - 1) % 3;
    //     debug("r: %d, c: %d", r, c);

    //     if (game->game_board[r][c] != 0){
    //         free(move);
    //         return NULL;
    //     }
    //     switch (role)
    //     {

    //     case FIRST_PLAYER_ROLE:
    //         game->game_board[r][c] = 1;
    //         break;

    //     case SECOND_PLAYER_ROLE:
    //         game->game_board[r][c] = -1;
    //         break;

    //     default:
    //         break;
    //     }
    // }
    // return move;
};

/*
 * Get a string that describes a specified GAME_MOVE, in a format
 * appropriate to be shown to human users.  The returned string should
 * be in a format from which the GAME_MOVE can be recovered by applying
 * game_parse_move() to it.  The returned string is in malloc'ed storage,
 * which it is the responsibility of the caller to free when it is no
 * longer needed.
 *
 * @param move  The GAME_MOVE whose description is to be obtained.
 * @return  A string describing the specified GAME_MOVE.
 */
char *game_unparse_move(GAME_MOVE *move)
{
    debug("%d", __LINE__);


    if (move == NULL)
    {
        return NULL;
    }
    char *str = malloc(sizeof(char) * 100);
    if (str == NULL)
    {
        return NULL;
    }
    switch (move->role)
    {

    case FIRST_PLAYER_ROLE:
        sprintf(str, "X HAS MOVED IN POSITION %d", move->value);
        break;

    case SECOND_PLAYER_ROLE:
        sprintf(str, "O HAS MOVED IN POSITION %d", move->value);
        break;

    default:
        break;
    }

    return str;
};
