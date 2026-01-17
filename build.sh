#!/bin/bash

# Build the docker image if it doesn't exist
docker build -t mesaos-builder .

# Run the build
docker run --rm -v $(pwd):/os mesaos-builder
