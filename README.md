# surveillance — Démonstration de persistance via supply chain (T1543.002)

Ce projet illustre, à des fins pédagogiques, un vecteur de supply chain attack documenté par MITRE ATT&CK sous la technique **T1543.002 (Create or Modify System Process: Systemd Service)**. Il compile un petit démon en C, le rend persistant via un service Systemd, puis empaquette l'ensemble dans un paquet Debian (`.deb`) dont le script d'installation s'exécute automatiquement en root.

## Sommaire

- [Objectifs pédagogiques](#objectifs-pédagogiques)
- [Architecture du projet](#architecture-du-projet)
- [Prérequis](#prérequis)
- [Job 1 — Compilation du démon C](#job-1--compilation-du-démon-c)
- [Job 2 — Persistance via Systemd](#job-2--persistance-via-systemd)
- [Job 3 — Empaquetage Debian](#job-3--empaquetage-debian)
- [Validation / Test](#validation--test)
- [Investigation SOC](#investigation-soc)
- [Désinstallation](#désinstallation)
- [Avertissement](#avertissement)

## Objectifs pédagogiques

- Comprendre les 4 étapes de la chaîne de compilation GCC (préprocesseur, assembleur, fichier objet, édition de liens).
- Mettre en place un service persistant avec Systemd.
- Comprendre le mécanisme d'exécution automatique en root d'un script `postinst` lors de l'installation d'un paquet Debian.
- Illustrer concrètement un vecteur de supply chain attack, et les moyens de détection côté SOC (`journalctl`, logs `dpkg`).

## Architecture du projet

```
.
├── surveillance.c                          # Code source du démon
├── surveillance                            # Binaire compilé
├── paquet-surveillance/
│   ├── DEBIAN/
│   │   ├── control                         # Métadonnées du paquet
│   │   └── postinst                        # Script post-installation (exécuté en root)
│   ├── usr/local/bin/
│   │   └── surveillance                    # Binaire embarqué dans le paquet
│   └── etc/systemd/system/
│       └── surveillance.service            # Unité systemd embarquée
└── README.md
```

## Prérequis

- Une distribution Linux basée sur Debian/Ubuntu (ou WSL avec systemd activé).
- `gcc` et les outils de base (`build-essential`).
- `dpkg-deb` (inclus par défaut sur Debian/Ubuntu).
- Droits `sudo`.

```bash
sudo apt update
sudo apt install build-essential -y
```

## Job 1 — Compilation du démon C

Le programme tourne en boucle infinie, affiche un message toutes les 3 secondes, et force l'écriture immédiate de sa sortie (`fflush`) pour que Systemd puisse capter ses messages.

```c
#include <stdio.h>
#include <unistd.h>

int main(void) {
    while (1) {
        printf("Le paquet fonctionne\n");
        fflush(stdout);
        sleep(3);
    }
    return 0;
}
```

Compilation en 4 étapes distinctes, pour observer chaque transformation :

```bash
# 1. Préprocesseur — expansion des #include
gcc -E surveillance.c -o surveillance.i

# 2. Traduction en assembleur
gcc -S surveillance.i -o surveillance.s

# 3. Génération du fichier objet
gcc -c surveillance.s -o surveillance.o

# 4. Édition de liens — résolution des symboles externes (printf, sleep, fflush)
gcc surveillance.o -o surveillance
```

Test :

```bash
./surveillance
```

## Job 2 — Persistance via Systemd

Déploiement du binaire :

```bash
sudo cp surveillance /usr/local/bin/
sudo chmod +x /usr/local/bin/surveillance
```

Fichier d'unité `/etc/systemd/system/surveillance.service` :

```ini
[Unit]
Description=Surveillance Monitor Daemon
After=network.target

[Service]
ExecStart=/usr/local/bin/surveillance
Restart=always
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=multi-user.target
```

Activation :

```bash
sudo systemctl daemon-reload
sudo systemctl enable surveillance.service
sudo systemctl start surveillance.service
sudo systemctl status surveillance.service
```

## Job 3 — Empaquetage Debian

Construction de l'arborescence du paquet :

```bash
mkdir -p paquet-surveillance/DEBIAN
mkdir -p paquet-surveillance/usr/local/bin
mkdir -p paquet-surveillance/etc/systemd/system

cp surveillance paquet-surveillance/usr/local/bin/
cp /etc/systemd/system/surveillance.service paquet-surveillance/etc/systemd/system/

chmod 755 paquet-surveillance/usr/local/bin/surveillance
chmod 644 paquet-surveillance/etc/systemd/system/surveillance.service
```

Fichier `paquet-surveillance/DEBIAN/control` :

```
Package: surveillance
Version: 1.0
Architecture: amd64
Maintainer: Ton Nom <ton.email@example.com>
Description: Service de supervision surveillance
 Un petit démon de démonstration illustrant la persistance
 via systemd et l'empaquetage Debian.
```

Script `paquet-surveillance/DEBIAN/postinst` — s'exécute automatiquement en root à l'installation :

```bash
#!/bin/sh
set -e

systemctl daemon-reload
systemctl enable surveillance.service
systemctl start surveillance.service

echo "Installation réussie : surveillance est actif."
```

```bash
chmod +x paquet-surveillance/DEBIAN/postinst
```

Construction du `.deb` :

```bash
dpkg-deb --build paquet-surveillance
```

## Validation / Test

Remise à l'état initial, puis installation du paquet pour vérifier que tout se déploie automatiquement, sans action manuelle :

```bash
# Remise à zéro
sudo systemctl stop surveillance.service
sudo systemctl disable surveillance.service
sudo rm /etc/systemd/system/surveillance.service
sudo rm /usr/local/bin/surveillance
sudo systemctl daemon-reload

# Installation du paquet
sudo dpkg -i paquet-surveillance.deb

# Vérification — le service doit être actif sans action manuelle
systemctl status surveillance.service
```

## Investigation SOC

Un analyste peut détecter et suivre l'activité du service via le journal centralisé de Systemd :

```bash
journalctl -u surveillance.service -f
```

- `-u surveillance.service` : filtre les logs pour cette unité uniquement.
- `-f` : suit les nouvelles entrées en direct (comme `tail -f`).

D'autres commandes utiles pour l'audit :

```bash
journalctl -u surveillance.service --since "10 min ago"
journalctl -u surveillance.service -b
cat /var/log/dpkg.log | grep surveillance
```

## Désinstallation

```bash
sudo dpkg -r surveillance
```

## Avertissement

Ce projet est réalisé dans un cadre strictement pédagogique, pour illustrer un mécanisme de persistance et de supply chain attack documenté (MITRE ATT&CK T1543.002). Le binaire fourni n'exécute aucune action malveillante — l'objectif est de démontrer que le véritable point de vigilance se situe au niveau du script `postinst`, qui s'exécute automatiquement en root sans confirmation de l'utilisateur lors de l'installation d'un paquet, quel que soit son contenu.

## Ressources

- `man gcc` — options `-E`, `-S`, `-c` et édition de liens
- `man systemd.service`, `man systemctl`
- `man journalctl` — filtrage des journaux par unité
- `man dpkg-deb`, Debian Policy Manual (structure `DEBIAN/control`, `postinst`)
- [MITRE ATT&CK — T1543.002](https://attack.mitre.org/techniques/T1543/002/)
