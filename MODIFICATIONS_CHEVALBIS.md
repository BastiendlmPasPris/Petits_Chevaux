# Documentation des modifications de chevalbis.c

## Vue d'ensemble
Ce document détaille toutes les modifications apportées au fichier `chevalbis.c` pour implémenter la gestion complète d'un cheval dans la course des Petits Chevaux avec mémoire partagée.

---

## 1. Déclaration des variables pour la mémoire partagée

### Localisation : Lignes 20-25

### Code ajouté :
```c
int cle_piste ;
piste_t * piste = NULL ;
int shm_piste_id ;              // ← AJOUTÉ

int cle_liste ;
liste_t * liste = NULL ;        // ← DÉCOMMENTÉ (était en commentaire)
int shm_liste_id ;              // ← AJOUTÉ
```

### Explication :
- **`shm_piste_id`** : Identifiant du segment de mémoire partagée pour la piste
  - Obtenu via `shmget()`
  - Permet à tous les chevaux de partager la même piste

- **`liste`** : Pointeur vers la liste des chevaux en mémoire partagée
  - Était commentée dans le code original
  - Maintenant active pour gérer l'enregistrement des chevaux

- **`shm_liste_id`** : Identifiant du segment de mémoire partagée pour la liste
  - Obtenu via `shmget()`
  - Permet à tous les chevaux de partager la même liste

---

## 2. Attachement de la piste en mémoire partagée

### Localisation : Lignes 84-101

### Code ajouté :
```c
/*
 * Attachement de la piste en memoire partagee
 */

shm_piste_id = shmget( cle_piste , sizeof(piste_t) , 0666 );
if( shm_piste_id == -1 )
  {
    fprintf( stderr, "%s : erreur attachement memoire partagee piste (cle=%d)\n" ,
             tab_arg[0]  , cle_piste );
    exit(-3);
  }

piste = (piste_t *) shmat( shm_piste_id , NULL , 0 );
if( piste == (piste_t *)-1 )
  {
    fprintf( stderr, "%s : erreur shmat piste\n" , tab_arg[0] );
    exit(-4);
  }
```

### Explication détaillée :

#### Étape 1 : `shmget()`
```c
shm_piste_id = shmget( cle_piste , sizeof(piste_t) , 0666 );
```
- **Rôle** : Obtient l'identifiant d'un segment de mémoire partagée existant
- **Paramètres** :
  - `cle_piste` : Clé fournie en argument (permet d'identifier le segment)
  - `sizeof(piste_t)` : Taille du segment à attacher
  - `0666` : Permissions (lecture/écriture pour tous)
- **Retour** : Identifiant du segment ou -1 en cas d'erreur

#### Étape 2 : `shmat()`
```c
piste = (piste_t *) shmat( shm_piste_id , NULL , 0 );
```
- **Rôle** : Attache le segment de mémoire partagée à l'espace d'adressage du processus
- **Paramètres** :
  - `shm_piste_id` : Identifiant obtenu précédemment
  - `NULL` : Le système choisit l'adresse automatiquement
  - `0` : Mode par défaut (lecture/écriture)
- **Retour** : Pointeur vers la piste en mémoire partagée

#### Gestion des erreurs :
- Si `shmget()` échoue → message d'erreur avec la clé → sortie avec code -3
- Si `shmat()` échoue → message d'erreur → sortie avec code -4

---

## 3. Enregistrement du cheval dans la liste

### Localisation : Lignes 103-127

### Code ajouté :
```c
/*
 * Enregistrement du cheval dans la liste
 */

/* Attachement de la liste en memoire partagee */
shm_liste_id = shmget( cle_liste , sizeof(liste_t) , 0666 );
if( shm_liste_id == -1 )
  {
    fprintf( stderr, "%s : erreur attachement memoire partagee liste (cle=%d)\n" ,
             tab_arg[0]  , cle_liste );
    exit(-3);
  }

liste = (liste_t *) shmat( shm_liste_id , NULL , 0 );
if( liste == (liste_t *)-1 )
  {
    fprintf( stderr, "%s : erreur shmat liste\n" , tab_arg[0] );
    exit(-4);
  }

/* Ajout du cheval dans la liste */
if( liste_elem_ajouter( liste , elem_cheval ) != 0 )
  {
    fprintf( stderr, "%s : erreur ajout cheval dans la liste\n" , tab_arg[0] );
    exit(-5);
  }
```

### Explication détaillée :

#### Phase 1 : Attachement de la liste (similaire à la piste)
- `shmget()` avec `cle_liste` pour obtenir l'identifiant du segment
- `shmat()` pour attacher la liste à l'espace mémoire du processus
- Gestion des erreurs identique

#### Phase 2 : Ajout du cheval
```c
liste_elem_ajouter( liste , elem_cheval )
```
- **Rôle** : Ajoute l'élément `elem_cheval` à la fin de la liste partagée
- **Contenu de `elem_cheval`** :
  - `cell` : Informations du cheval (PID, marque)
  - `etat` : État initial = `EN_COURSE`
  - `sem` : Sémaphore (non encore initialisé ici)
- **Effet** : Le cheval est maintenant visible par tous les autres processus
- **Retour** : 0 si succès, autre valeur en cas d'erreur

#### Pourquoi c'est important :
- Permet aux autres chevaux de savoir qui est en course
- Nécessaire pour décaniller un cheval (modifier son état dans la liste)
- Permet de gérer la synchronisation entre processus

---

## 4. Vérification si le cheval n'est pas décanillé

### Localisation : Lignes 118-142

### Code ajouté :
```c
/*
 * Verif si pas decanille
 */

/* Recherche du cheval dans la liste */
int ind_cheval ;
if( liste_elem_rechercher( &ind_cheval , liste , elem_cheval ) == VRAI )
  {
    /* Lecture de l'element dans la liste pour verifier son etat */
    elem_t elem_liste = liste_elem_lire( liste , ind_cheval );

    /* Test si decanille */
    if( elem_decanille( elem_liste ) == VRAI )
      {
        printf("Le cheval \"%c\" A ETE DECANILLE\n" , marque );
        fini = VRAI ;
        continue ; /* Sortie de la boucle */
      }
  }
else
  {
    fprintf( stderr , "%s : erreur, cheval \"%c\" non trouve dans la liste\n" ,
             tab_arg[0] , marque );
    exit(-6);
  }
```

### Explication détaillée :

#### Étape 1 : Recherche du cheval
```c
liste_elem_rechercher( &ind_cheval , liste , elem_cheval )
```
- **Rôle** : Cherche un élément dans la liste en comparant les cellules (PID + marque)
- **Paramètres** :
  - `&ind_cheval` : Pointeur pour stocker l'indice trouvé
  - `liste` : La liste partagée
  - `elem_cheval` : L'élément à rechercher (notre cheval)
- **Retour** : `VRAI` si trouvé, `FAUX` sinon

#### Étape 2 : Lecture de l'état actuel
```c
elem_t elem_liste = liste_elem_lire( liste , ind_cheval );
```
- **Rôle** : Lit l'élément à l'indice trouvé
- **Important** : L'état peut avoir changé depuis l'enregistrement (un autre cheval a pu décaniller celui-ci)

#### Étape 3 : Test de décanillage
```c
elem_decanille( elem_liste )
```
- **Rôle** : Vérifie si `elem_liste.etat == DECANILLE`
- **Si décanillé** :
  - Affiche un message
  - Met `fini = VRAI` pour sortir de la boucle
  - `continue` pour passer à l'itération suivante (qui sortira)

#### Gestion d'erreur :
- Si le cheval n'est pas trouvé → erreur grave → sortie avec code -6
- Cela ne devrait jamais arriver (le cheval a été ajouté au début)

#### Pourquoi à chaque tour ?
À chaque tour, le cheval vérifie s'il n'a pas été décanillé par un autre cheval entre temps. C'est essentiel dans un système concurrent.

---

## 5. Gestion du décanillage d'un cheval victime

### Localisation : Lignes 167-193

### Code ajouté :
```c
/*
 * Si case d'arrivee occupee alors on decanille le cheval existant
 */

if( piste_cell_occupee( piste , arrivee ) == VRAI )
  {
    /* Lecture de la cellule occupee */
    cell_t cell_victime ;
    piste_cell_lire( piste , arrivee , &cell_victime );

    /* Recherche du cheval victime dans la liste */
    elem_t elem_victime ;
    elem_cell_affecter( &elem_victime , cell_victime );
    elem_etat_affecter( &elem_victime , EN_COURSE ); /* Pour la recherche */

    int ind_victime ;
    if( liste_elem_rechercher( &ind_victime , liste , elem_victime ) == VRAI )
      {
        /* Decaniller le cheval victime dans la liste */
        liste_elem_decaniller( liste , ind_victime );

#ifdef _DEBUG_
        printf("Le cheval \"%c\" decanille le cheval \"%c\"\n",
               marque , cell_marque_lire(cell_victime) );
#endif
      }
  }
```

### Explication détaillée :

#### Étape 1 : Vérification si la case est occupée
```c
piste_cell_occupee( piste , arrivee )
```
- **Rôle** : Vérifie si la case d'arrivée contient déjà un cheval
- **Implémentation** : Teste si `cell.pid != 0`
- **Retour** : `VRAI` si occupée, `FAUX` si libre

#### Étape 2 : Lecture de la cellule victime
```c
piste_cell_lire( piste , arrivee , &cell_victime );
```
- **Rôle** : Récupère les informations du cheval présent sur la case
- **Contenu de `cell_victime`** : PID et marque du cheval victime

#### Étape 3 : Création d'un élément de recherche
```c
elem_cell_affecter( &elem_victime , cell_victime );
elem_etat_affecter( &elem_victime , EN_COURSE );
```
- **Rôle** : Construit un élément avec la cellule lue pour rechercher dans la liste
- **État = EN_COURSE** : Pour la recherche (on cherche un cheval qui était en course)

#### Étape 4 : Recherche et décanillage
```c
liste_elem_rechercher( &ind_victime , liste , elem_victime )
```
- Trouve l'indice du cheval victime dans la liste

```c
liste_elem_decaniller( liste , ind_victime );
```
- **Rôle** : Change l'état du cheval victime à `DECANILLE` dans la liste
- **Effet** : Au prochain tour, le cheval victime verra qu'il est décanillé et sortira de la course

#### Message de debug :
- Affiché uniquement si compilé avec `-D_DEBUG_`
- Indique qui décanille qui

#### Scénario complet :
1. Cheval A est en case 5
2. Cheval B arrive sur la case 5
3. Cheval B lit que A est sur la case
4. Cheval B trouve A dans la liste
5. Cheval B met l'état de A à DECANILLE
6. Au prochain tour, A verra qu'il est décanillé et sortira

---

## 6. Déplacement du cheval sur la piste

### Localisation : Lignes 195-206

### Code ajouté :
```c
/*
 * Deplacement: effacement case de depart, affectation case d'arrivee
 */

piste_cell_effacer( piste , depart );
commun_attendre_fin_saut();
piste_cell_affecter( piste , arrivee , cell_cheval );

#ifdef _DEBUG_
  printf("Deplacement du cheval \"%c\" de %d a %d\n",
         marque, depart, arrivee );
#endif
```

### Explication détaillée :

#### Étape 1 : Effacement de la case de départ
```c
piste_cell_effacer( piste , depart );
```
- **Rôle** : Met le PID de la case à 0 (case vide)
- **Effet** : La case de départ devient libre
- **Important** : Fait AVANT l'affectation pour éviter d'avoir le cheval à deux endroits

#### Étape 2 : Attente du saut
```c
commun_attendre_fin_saut();
```
- **Rôle** : Simule le temps que prend le cheval pour sauter de case en case
- **Durée** : Définie par `TEMPS_SAUT` (2 secondes par défaut)
- **Effet visuel** : Permet de voir le déplacement progressif

#### Étape 3 : Affectation à la case d'arrivée
```c
piste_cell_affecter( piste , arrivee , cell_cheval );
```
- **Rôle** : Place le cheval sur la nouvelle case
- **Paramètres** :
  - `piste` : La piste partagée
  - `arrivee` : Indice de la case d'arrivée
  - `cell_cheval` : Cellule contenant PID et marque

#### Séquence temporelle :
```
t=0 : Case départ=[A], Case arrivée=[ ]
      ↓ piste_cell_effacer(depart)
t=1 : Case départ=[ ], Case arrivée=[ ]
      ↓ commun_attendre_fin_saut() (attend 2 sec)
t=3 : Case départ=[ ], Case arrivée=[ ]
      ↓ piste_cell_affecter(arrivee)
t=3 : Case départ=[ ], Case arrivée=[A]
```

#### Note sur la concurrence :
- Entre l'effacement et l'affectation, le cheval n'est nulle part sur la piste
- Un autre cheval pourrait théoriquement prendre la case d'arrivée pendant ce temps
- C'est pourquoi on vérifie l'occupation AVANT le déplacement

---

## 7. Suppression du cheval de la liste

### Localisation : Lignes 220-248

### Code ajouté :
```c
/*
 * Suppression du cheval de la liste
 */

/* Recherche du cheval dans la liste */
int ind_cheval_final ;
if( liste_elem_rechercher( &ind_cheval_final , liste , elem_cheval ) == VRAI )
  {
    /* Suppression du cheval de la liste */
    if( liste_elem_supprimer( liste , ind_cheval_final ) != 0 )
      {
        fprintf( stderr, "%s : erreur suppression cheval \"%c\" de la liste\n" ,
                 tab_arg[0] , marque );
        exit(-7);
      }
#ifdef _DEBUG_
    printf("Le cheval \"%c\" a ete supprime de la liste\n" , marque );
#endif
  }
else
  {
    fprintf( stderr , "%s : erreur, cheval \"%c\" non trouve dans la liste pour suppression\n" ,
             tab_arg[0] , marque );
    exit(-8);
  }

/* Detachement de la memoire partagee */
shmdt( piste );
shmdt( liste );
```

### Explication détaillée :

#### Phase 1 : Recherche du cheval
```c
liste_elem_rechercher( &ind_cheval_final , liste , elem_cheval )
```
- Identique à la recherche dans la boucle
- Trouve l'indice du cheval dans la liste

#### Phase 2 : Suppression
```c
liste_elem_supprimer( liste , ind_cheval_final )
```
- **Rôle** : Retire l'élément de la liste et décale les éléments suivants
- **Algorithme** :
  ```
  Liste avant : [A, B, C, D]  (suppression de B à l'indice 1)
  1. Décalage : A reste, C → position 1, D → position 2
  Liste après : [A, C, D, ?]
  2. Décrémentation : nb_elements = 3
  Liste finale : [A, C, D]
  ```
- **Retour** : 0 si succès

#### Phase 3 : Détachement de la mémoire partagée
```c
shmdt( piste );
shmdt( liste );
```
- **Rôle** : Détache les segments de mémoire partagée de l'espace d'adressage du processus
- **Important** : Ne détruit PAS les segments (d'autres chevaux peuvent encore les utiliser)
- **Effet** : Libère les ressources du processus

#### Pourquoi supprimer à la fin ?
1. **Si le cheval termine** : Il n'a plus besoin d'être dans la liste
2. **Si le cheval est décanillé** : Son état reste DECANILLE mais il sort du programme
3. **Nettoyage** : Évite d'avoir des chevaux "fantômes" dans la liste

#### Gestion des erreurs :
- Si non trouvé → erreur -8 (ne devrait jamais arriver)
- Si suppression échoue → erreur -7

---

## Résumé du cycle de vie complet d'un cheval

```
1. INITIALISATION
   ├─ Lecture des arguments (clés, marque)
   ├─ Création de cell_cheval (PID + marque)
   └─ Création de elem_cheval (cell + état EN_COURSE)

2. ATTACHEMENT MÉMOIRE PARTAGÉE
   ├─ shmget() + shmat() pour la piste
   └─ shmget() + shmat() pour la liste

3. ENREGISTREMENT
   └─ liste_elem_ajouter() → Le cheval est visible par tous

4. BOUCLE DE COURSE (tant que !fini)
   ├─ Attente du tour (TEMPS_TOUR = 5 sec)
   │
   ├─ VÉRIFICATION DÉCANILLAGE
   │  ├─ Recherche dans la liste
   │  ├─ Lecture de l'état actuel
   │  └─ Si DECANILLE → fini = VRAI, continue
   │
   ├─ COUP DE DÉ
   │  ├─ deplacement = random(1-6)
   │  ├─ arrivee = depart + deplacement
   │  └─ Si arrivee > 19 → arrivee = 19, fini = VRAI
   │
   └─ DÉPLACEMENT (si depart != arrivee)
      ├─ SI case d'arrivée occupée
      │  ├─ Lecture de cell_victime
      │  ├─ Recherche victime dans liste
      │  └─ liste_elem_decaniller(victime)
      │
      ├─ piste_cell_effacer(depart)
      ├─ commun_attendre_fin_saut() (2 sec)
      ├─ piste_cell_affecter(arrivee)
      └─ piste_afficher_lig()

5. FIN DE COURSE
   ├─ Message "A FRANCHIT LA LIGNE D'ARRIVEE"
   │  (ou "A ETE DECANILLE")
   │
   ├─ SUPPRESSION DE LA LISTE
   │  ├─ Recherche du cheval
   │  └─ liste_elem_supprimer()
   │
   └─ DÉTACHEMENT MÉMOIRE
      ├─ shmdt(piste)
      └─ shmdt(liste)

6. EXIT(0)
```

---

## Codes de sortie du programme

| Code | Signification |
|------|---------------|
| 0 | Succès (cheval terminé ou décanillé) |
| -1 | Nombre d'arguments incorrect |
| -2 | Mauvais format de clé ou marque |
| -3 | Erreur shmget() (piste ou liste) |
| -4 | Erreur shmat() (piste ou liste) |
| -5 | Erreur ajout cheval dans la liste |
| -6 | Cheval non trouvé lors de la vérification |
| -7 | Erreur suppression cheval de la liste |
| -8 | Cheval non trouvé lors de la suppression |

---

## Concepts clés de programmation concurrente utilisés

### 1. Mémoire partagée (Shared Memory)
- **Segments IPC** : Piste et liste partagées entre tous les processus
- **Avantage** : Communication rapide (pas de copie de données)
- **Attention** : Nécessite de la synchronisation (non implémentée ici avec sémaphores)

### 2. États partagés
- **État EN_COURSE** : Le cheval peut continuer
- **État DECANILLE** : Le cheval doit s'arrêter
- **État ARRIVE** : Le cheval a fini (non utilisé ici)

### 3. Conditions de course potentielles
- **Problème** : Deux chevaux peuvent lire/écrire en même temps
- **Exemple** :
  - Cheval A lit case 5 → libre
  - Cheval B lit case 5 → libre
  - Cheval A écrit case 5
  - Cheval B écrit case 5 → COLLISION (un des deux écrase l'autre)
- **Solution future** : Utiliser des sémaphores (elem.sem)

### 4. Atomicité des opérations
- `liste_elem_ajouter()` n'est pas atomique → risque de corruption si deux chevaux s'ajoutent simultanément
- `liste_elem_decaniller()` n'est pas atomique → risque de race condition
- **Note** : Dans un vrai système, il faudrait protéger ces sections critiques

---

## Améliorations possibles

1. **Synchronisation avec sémaphores**
   - Protéger l'accès à la liste
   - Protéger l'accès à chaque case de la piste
   - Utiliser `elem.sem` pour synchroniser les opérations sur un cheval

2. **Gestion de l'état ARRIVE**
   - Mettre l'état à ARRIVE au lieu de supprimer immédiatement
   - Permettre d'afficher un classement final

3. **Effacement de la piste à l'arrivée**
   - Actuellement, le cheval reste sur la case 19
   - Pourrait être effacé après franchissement de la ligne

4. **Logs plus détaillés**
   - Horodatage des événements
   - Fichier de log pour déboguer

5. **Signal handlers**
   - Gérer SIGINT (Ctrl+C) pour nettoyer proprement
   - Supprimer le cheval de la liste si interruption

---

## Compilation et exécution

### Compilation standard
```bash
gcc -o chevalbis chevalbis.c liste.c piste.c cell.c elem.c commun.c -I.
```

### Compilation avec debug
```bash
gcc -D_DEBUG_ -o chevalbis chevalbis.c liste.c piste.c cell.c elem.c commun.c -I.
```

### Exécution
```bash
./chevalbis <cle_piste> <cle_liste> <marque>
```

Exemple :
```bash
./chevalbis 1234 5678 A
```

---

## Fichier créé le 2025-01-19
