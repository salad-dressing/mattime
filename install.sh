# Exit immediately if a command exits with a non-zero status
set -e


# define variables

SOURCE="mattime.c"
BINARY="mattime"
DATABASE="logs.db"
BINARY_DIRECTORY="/usr/local/bin"

USER_HOME=$(eval echo ~${SUDO_USER})
DATABASE_DIRECTORY="$USER_HOME/.local/share/mattime"

# set up clean environment

if [ -f "$BINARY_DIRECTORY/$BINARY" ]; then
  echo "Removing previous binary..."
  rm "$BINARY_DIRECTORY/$BINARY"
fi

if [ ! -f "$SOURCE" ]; then
  echo "Error: source file $SOURCE not found."
  exit 1
fi

if [ ! -f "$DATABASE_DIRECTORY/$DATABASE" ]; then
  echo "Creating database file $DATABASE..."
  mkdir -p "$DATABASE_DIRECTORY"
  touch "$DATABASE_DIRECTORY/$DATABASE"
fi

echo "Compiling binary..."
make

echo "Copying binary to $BINARY_DIRECTORY..."
cp -u "$BINARY" "$BINARY_DIRECTORY"

echo "Setting permissions..."
chmod 755 "$DATABASE_DIRECTORY"
chmod 755 "$BINARY_DIRECTORY/$BINARY"
chmod 755 "$DATABASE_DIRECTORY/$DATABASE"
chown $SUDO_USER:$SUDO_USER "$BINARY_DIRECTORY/$BINARY"
chown $SUDO_USER:$SUDO_USER "$USER_HOME/.local/share/mattime"
chown $SUDO_USER:$SUDO_USER "$DATABASE_DIRECTORY/$DATABASE"

echo ""
echo "Installation completed successfully."