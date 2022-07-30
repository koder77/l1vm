#!/bin/bash
echo "downloading key of koder77..."
wget https://midnight-koder.net/blog/assets/gpg-keys/spietzonke-gpg-pubkey.asc
echo "adding key to your keyring"
gpg --import spietzonke-gpg-pubkey.asc
