# Jeux Game Server

The Jeux Game Server is a multi-threaded server that facilitates online gaming using the Jeux protocol. This server allows players to log in, invite other players to games, make moves in games, and communicate with each other through a network connection.

## Introduction

The Jeux Game Server is designed to provide a platform for online multiplayer games. It handles the communication between clients and ensures that game-related actions are performed according to the Jeux protocol. This README provides an overview of the project, its features, and how to set it up and use it.

## Features

- Multi-threaded server: Handles multiple clients simultaneously.
- User registration: Allows users to create accounts and log in.
- User listing: Provides a list of currently logged-in users and their ratings.
- Game invitations: Supports sending and receiving game invitations.
- Game management: Handles game moves, resignations, and game outcomes.
- Secure communication: Ensures that communication between clients and the server adheres to the Jeux protocol.

## Requirements

To run the Jeux Game Server, you'll need the following:

- A C compiler (e.g., GCC)
- The Jeux Game Server source code
- Dependencies (e.g., libraries, header files)


## Usage

- **Client Usage:**
  Clients can log in, list users, send invitations, make moves, and manage games following the Jeux protocol.

- **Server Usage:**
  The server handles incoming client connections, manages user registration, and facilitates game-related communication.

- **Logging:**
  The server logs important events and errors to aid in debugging and monitoring.


