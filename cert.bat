makecert -r -sv MyCert.pvk -n CN="MyCert" MyCert.cer
cert2spc MyCert.cer MyCert.spc
pvk2pfx -pvk MyCert.pvk -pi pswd -spc MyCert.spc -pfx MyCert.pfx -po pswd
signtool sign /fd sha256 /f MyCert.pfx /p pswd /t http://timestamp.digicert.com /v manager/FsCrypt.sys