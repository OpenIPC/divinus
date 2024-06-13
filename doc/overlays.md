## Overlays

### Usage example

Parameters for each region can be passed in one or many calls:
```
curl "http://192.168.1.17/api/osd/0?font=comic&size=32.0&text=Frontdoor"
curl http://192.168.1.17/api/osd/0?posy=72
```
N.B. Ampersands require the address to be enclosed with double quotes under Windows or to be escaped with a backslash under Unix OSes

Supported fonts (sourced from /usr/share/fonts/truetype/) can render Unicode characters:
```
curl http://192.168.1.17/api/osd/0?text=Entrée
```

Empty strings are used to clear the regions:
```
curl http://192.168.1.17/api/osd/1?text=
```

Specifiers starting with a dollar sign are used to represent real-time statistics:
```
curl http://192.168.1.17/api/osd/1?text=$B%20C:$C%20M:$M
```
N.B. Spaces have to be escaped with %20 in curl URL syntaxes

Showing the time and customizing the time format is done this way:
```
curl http://192.168.1.17/api/time?fmt=%25Y/%25m/%25d%20%25H:%25M:%25S
curl http://192.168.1.17/api/osd/2?text=$t&posy=120
```
N.B. Percent signs have to be escaped with %25 in curl URL syntaxes

UTC date and time can be set using Unix timestamps:
```
curl http://192.168.1.17/api/time?ts=1712320920
```

24- and 32-bit bitmap files (.bmp) can be uploaded to a region using this command:
```
curl -F data=@.\Desktop\myimage.bmp http://192.168.1.17/api/osd/3
```
N.B. curl already implies "-X POST" when passing a file with "-F"