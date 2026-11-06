# Deploying the integrated server

This repository contains the AzerothCore playerbot fork, all local modules including
`modules/mod-playerbots` (the ADVAI checkout), SQL migrations, and the matching server
DBC snapshot in `data/dbc`. The DBC checksums are recorded in `data/dbc/MANIFEST.json`.

## Server setup

1. Install the build and MySQL prerequisites documented in `doc/` and the AzerothCore
   Linux server setup guide. This fork and its custom modules must be built together.
2. Extract `maps`, `vmaps`, `mmaps`, and `Cameras` from the **matching customized client**,
   using this source tree's extractor tools. Copy them beside the supplied `dbc` directory
   into a server data directory, and point `DataDir` in `worldserver.conf` at that directory.
   Standard retail DBCs do not replace the integrated custom DBCs in this repository.
3. Configure and build with CMake, enabling the included modules (for example,
   `-DMODULES=static`). Install the resulting authserver and worldserver. Copy the
   distributed `.conf.dist` files to their actual `.conf` names and set database connection
   details locally; never commit production credentials.
4. Create the auth, characters, world and playerbots databases, configure their connection
   strings, then run the core and module database updater with the same module set used for
   the server build. Review module-specific migrations and optional SQL before applying them.
5. Install the playerbot module configuration under the server's `etc/modules/` directory.
   Merge `modules/mod-playerbots/conf/playerbots.advai.preset` if ADVAI is wanted; configure
   LLM service and the optional LLM preset separately. See `DEBIAN_DEPLOY_NOTES.md` and the
   playerbot README for the known configuration and operational requirements.
6. Set the realm's public address in the auth database and distribute a matching client
   configured to connect to it. Start the servers, inspect logs and validate the realm before
   allowing players to connect.

The repository does not contain live account/character databases, production credentials,
client archives, or extracted map/vmap/mmap data. The full Linux link, database migration and
realm startup must be validated on the target host; see `DEBIAN_DEPLOY_NOTES.md`.
