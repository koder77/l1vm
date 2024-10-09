This is some new SSL socket code demo.

To create a SSL certificate with name "mycert.pem":

$ openssl req -x509 -nodes -days 365 -newkey rsa:4096 -keyout mycert.pem -out mycert.pem

Run the SSL server demo program first. Then the client.
If everything works, then the server should show "Hello world!"

Have fun!
