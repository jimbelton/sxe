Instructions for creating OpenSSL Keys/Certificates
===================================================

Create a new 1,024-bit RSA private key (unencrypted)
----------------------------------------------------

$ openssl genrsa -out privkey.pem 1024

Generate a certificate signing request (CSR)
--------------------------------------------

$ cat > req.txt <<EOF
[ req ]
distinguished_name  = my_distinguished_name
prompt              = no
utf8                = yes
string_mask         = utf8only

[ my_distinguished_name ]
CN                  = *.sophoswdx.com
O                   = Engineering
C                   = CA
ST                  = British Columbia
L                   = Vancouver
EOF

$ openssl req -batch -new -key privkey.pem -out csr.txt -config req.txt

Generate a self-signed certificate using the CSR and Private Key
----------------------------------------------------------------

$ openssl x509 -req -days 365 -in csr.txt -signkey privkey.pem -out cert.pem

Validate that the certificate is what you expected
--------------------------------------------------

$ openssl x509 -in cert.pem -text -noout

