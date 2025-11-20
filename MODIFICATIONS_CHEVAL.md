# Modifications pour cheval.c

Ce document détaille les modifications à apporter au fichier `cheval.c` pour compléter les sections manquantes. Chaque partie est expliquée avec le code à ajouter et les justifications.

---

## 1. Enregistrement du cheval dans la liste (ligne 83-86)

### Code à ajouter :

```c
/*
 * Enregistrement du cheval dans la liste
 */

/* Attachement au segment de mémoire partagée de la liste */
int id_liste = shmget(cle_liste, sizeof(liste_t), 0666);
if (id_liste == -1) {
    perror("cheval: Erreur shmget pour la liste");
    exit(-3);
}

liste_t *liste = (liste_t *)shmat(id_liste, NULL, 0);
if (liste == (liste_t *)-1) {
    perror("cheval: Erreur shmat pour la liste");
    exit(-4);
}

/* Récupération du sémaphore protégeant la liste */
int sem_liste = semget(cle_liste, 1, 0666);
if (sem_liste == -1) {
    perror("cheval: Erreur semget pour le sémaphore de la liste");
    exit(-5);
}

/* Création du sémaphore individuel du cheval */
if (elem_sem_creer(&elem_cheval) == -1) {
    fprintf(stderr, "cheval: Erreur création du sémaphore du cheval\n");
    exit(-6);
}

/* Protection de la section critique: accès à la liste */
struct sembuf op_P = {0, -1, SEM_UNDO};
if (semop(sem_liste, &op_P, 1) == -1) {
    perror("cheval: Erreur verrouillage liste");
    exit(-7);
}

/* Ajout du cheval dans la liste */
if (liste_elem_ajouter(liste, elem_cheval) == -1) {
    fprintf(stderr, "cheval: Erreur ajout du cheval dans la liste\n");
    exit(-8);
}

/* Libération de la section critique */
struct sembuf op_V = {0, +1, SEM_UNDO};
if (semop(sem_liste, &op_V, 1) == -1) {
    perror("cheval: Erreur déverrouillage liste");
    exit(-9);
}

/* Attachement au segment de mémoire partagée de la piste */
int id_piste = shmget(cle_piste, sizeof(piste_t), 0666);
if (id_piste == -1) {
    perror("cheval: Erreur shmget pour la piste");
    exit(-10);
}

piste = (piste_t *)shmat(id_piste, NULL, 0);
if (piste == (piste_t *)-1) {
    perror("cheval: Erreur shmat pour la piste");
    exit(-11);
}

/* Récupération de l'ensemble de sémaphores de la piste */
int sem_piste = semget(cle_piste, PISTE_LONGUEUR, 0666);
if (sem_piste == -1) {
    perror("cheval: Erreur semget pour les sémaphores de la piste");
    exit(-12);
}
```

### Explications :

**Objectif** : Enregistrer le cheval dans la liste partagée et s'attacher aux ressources (liste et piste).

**Détails** :
1. **Attachement à la liste** : Utilisation de `shmget()` pour obtenir l'identifiant du segment de mémoire partagée de la liste, puis `shmat()` pour s'y attacher.
2. **Sémaphore de la liste** : Récupération du sémaphore qui protège l'accès à la liste (exclusion mutuelle).
3. **Sémaphore individuel** : Création d'un sémaphore propre au cheval avec `elem_sem_creer()` pour éviter qu'il soit décanillé pendant un déplacement.
4. **Section critique** : Verrouillage du sémaphore de la liste (opération P) avant d'ajouter le cheval, puis déverrouillage (opération V).
5. **Attachement à la piste** : Même processus que pour la liste, avec récupération de l'ensemble des sémaphores de la piste (un par case).

---

## 2. Vérification si pas décanillé (ligne 94-97)

### Code à ajouter :

```c
/*
 * Verif si pas decanille
 */

/* Protection: accès à la liste pour lire l'état */
struct sembuf op_P_liste = {0, -1, SEM_UNDO};
if (semop(sem_liste, &op_P_liste, 1) == -1) {
    perror("cheval: Erreur verrouillage liste");
    break;
}

/* Recherche du cheval dans la liste */
int ind_cheval;
if (liste_elem_rechercher(&ind_cheval, liste, elem_cheval) == FAUX) {
    fprintf(stderr, "cheval '%c': Erreur, cheval non trouvé dans la liste\n", marque);
    struct sembuf op_V_liste = {0, +1, SEM_UNDO};
    semop(sem_liste, &op_V_liste, 1);
    break;
}

/* Lecture de l'élément mis à jour */
elem_cheval = liste_elem_lire(liste, ind_cheval);

/* Libération de la liste */
struct sembuf op_V_liste = {0, +1, SEM_UNDO};
if (semop(sem_liste, &op_V_liste, 1) == -1) {
    perror("cheval: Erreur déverrouillage liste");
    break;
}

/* Test si le cheval a été décanillé */
if (elem_decanille(elem_cheval) == VRAI) {
    printf("Le cheval \"%c\" A ETE DECANILLE!\n", marque);
    fini = VRAI;
    continue;
}
```

### Explications :

**Objectif** : Vérifier à chaque tour si le cheval n'a pas été décanillé par un autre cheval.

**Détails** :
1. **Accès sécurisé** : Verrouillage de la liste avant de lire l'état du cheval.
2. **Recherche** : Utilisation de `liste_elem_rechercher()` pour retrouver le cheval dans la liste par sa cellule (pid + marque).
3. **Lecture de l'état** : Récupération de l'élément mis à jour qui contient l'état actuel (EN_COURSE, DECANILLE, ou ARRIVE).
4. **Test de décanillage** : Si l'état est DECANILLE, on arrête la course du cheval et on sort de la boucle.
5. **Continue** : Si décanillé, on passe directement à la fin de la boucle pour sortir proprement.

---

## 3. Avancée sur la piste (ligne 100-103)

### Code à ajouter :

```c
/*
 * Avancee sur la piste
 */

/* Verrouillage du sémaphore du cheval pour éviter d'être décanillé pendant le déplacement */
if (elem_sem_verrouiller(&elem_cheval) == -1) {
    fprintf(stderr, "cheval '%c': Erreur verrouillage sémaphore du cheval\n", marque);
    fini = VRAI;
    break;
}
```

### Explications :

**Objectif** : Protéger le cheval contre le décanillage pendant son déplacement (pendant le "saut").

**Détails** :
1. **Sémaphore individuel** : Chaque cheval possède son propre sémaphore (créé à l'enregistrement).
2. **Verrouillage** : En prenant son sémaphore avant de se déplacer, le cheval garantit qu'aucun autre cheval ne pourra le décaniller pendant qu'il est "en l'air" (entre la case de départ et d'arrivée).
3. **Principe** : Si un autre cheval C1 veut décaniller ce cheval C2, C1 devra d'abord prendre le sémaphore de C2. Si C2 est en déplacement, C1 devra attendre que C2 ait fini son saut.

---

## 4. Si case d'arrivée occupée alors on décanille le cheval existant (ligne 121-124)

### Code à ajouter :

```c
/*
 * Si case d'arrivee occupee alors on decanille le cheval existant
 */

/* Verrouillage de la case d'arrivée */
struct sembuf op_P_arrivee = {arrivee, -1, SEM_UNDO};
if (semop(sem_piste, &op_P_arrivee, 1) == -1) {
    perror("cheval: Erreur verrouillage case d'arrivée");
    elem_sem_deverrouiller(&elem_cheval);
    fini = VRAI;
    break;
}

/* Vérification si la case d'arrivée est occupée */
if (piste_cell_occupee(piste, arrivee) == VRAI) {
    /* Lecture de la cellule du cheval à décaniller */
    cell_t cell_victime;
    piste_cell_lire(piste, arrivee, &cell_victime);

    /* Création d'un élément temporaire pour rechercher la victime dans la liste */
    elem_t elem_victime;
    elem_cell_affecter(&elem_victime, cell_victime);

    /* Accès à la liste pour décaniller la victime */
    struct sembuf op_P_liste_decanille = {0, -1, SEM_UNDO};
    if (semop(sem_liste, &op_P_liste_decanille, 1) == -1) {
        perror("cheval: Erreur verrouillage liste pour décanillage");
        struct sembuf op_V_arrivee = {arrivee, +1, SEM_UNDO};
        semop(sem_piste, &op_V_arrivee, 1);
        elem_sem_deverrouiller(&elem_cheval);
        fini = VRAI;
        break;
    }

    /* Recherche de la victime dans la liste */
    int ind_victime;
    if (liste_elem_rechercher(&ind_victime, liste, elem_victime) == VRAI) {
        /* Lecture de l'élément complet de la victime */
        elem_t elem_victime_complet = liste_elem_lire(liste, ind_victime);

        /* Verrouillage du sémaphore de la victime avant de la décaniller */
        if (elem_sem_verrouiller(&elem_victime_complet) == -1) {
            fprintf(stderr, "cheval '%c': Erreur verrouillage sémaphore victime\n", marque);
        } else {
            /* Décanillage de la victime */
            liste_elem_decaniller(liste, ind_victime);
            printf("Le cheval \"%c\" DECANILLE le cheval \"%c\"!\n",
                   marque, cell_marque_lire(cell_victime));

            /* Déverrouillage du sémaphore de la victime */
            elem_sem_deverrouiller(&elem_victime_complet);
        }
    }

    /* Libération de la liste */
    struct sembuf op_V_liste_decanille = {0, +1, SEM_UNDO};
    semop(sem_liste, &op_V_liste_decanille, 1);
}
```

### Explications :

**Objectif** : Si la case d'arrivée est occupée par un autre cheval, le décaniller.

**Détails** :
1. **Verrouillage de la case** : Chaque case de la piste est protégée par un sémaphore (le sémaphore numéro i protège la case i). On verrouille la case d'arrivée.
2. **Test d'occupation** : Vérification si la case contient déjà un cheval avec `piste_cell_occupee()`.
3. **Identification de la victime** : Lecture de la cellule pour récupérer le pid et la marque du cheval à décaniller.
4. **Recherche dans la liste** : Création d'un élément temporaire pour retrouver la victime dans la liste partagée.
5. **Double verrouillage** :
   - Verrouillage de la liste pour accéder aux états
   - Verrouillage du sémaphore de la victime (pour respecter le protocole : on ne peut décaniller un cheval qu'en prenant son sémaphore)
6. **Décanillage** : Mise à jour de l'état à DECANILLE avec `liste_elem_decaniller()`.
7. **Libérations** : Déverrouillage des sémaphores dans l'ordre inverse.

---

## 5. Déplacement: effacement case de départ, affectation case d'arrivée (ligne 126-129)

### Code à ajouter :

```c
/*
 * Deplacement: effacement case de depart, affectation case d'arrivee
 */

/* Verrouillage de la case de départ si elle existe (pas pour le premier mouvement) */
if (depart > 0 || (depart == 0 && arrivee > 0)) {
    struct sembuf op_P_depart = {depart, -1, SEM_UNDO};
    if (semop(sem_piste, &op_P_depart, 1) == -1) {
        perror("cheval: Erreur verrouillage case de départ");
        struct sembuf op_V_arrivee_err = {arrivee, +1, SEM_UNDO};
        semop(sem_piste, &op_V_arrivee_err, 1);
        elem_sem_deverrouiller(&elem_cheval);
        fini = VRAI;
        break;
    }

    /* Effacement de la case de départ */
    piste_cell_effacer(piste, depart);

    /* Libération de la case de départ */
    struct sembuf op_V_depart = {depart, +1, SEM_UNDO};
    if (semop(sem_piste, &op_V_depart, 1) == -1) {
        perror("cheval: Erreur déverrouillage case de départ");
    }
}

/* Simulation du temps de saut entre départ et arrivée */
commun_attendre_fin_saut();

/* Affectation de la case d'arrivée */
piste_cell_affecter(piste, arrivee, cell_cheval);

/* Libération de la case d'arrivée */
struct sembuf op_V_arrivee_fin = {arrivee, +1, SEM_UNDO};
if (semop(sem_piste, &op_V_arrivee_fin, 1) == -1) {
    perror("cheval: Erreur déverrouillage case d'arrivée");
}

/* Déverrouillage du sémaphore du cheval (fin du déplacement) */
if (elem_sem_deverrouiller(&elem_cheval) == -1) {
    fprintf(stderr, "cheval '%c': Erreur déverrouillage sémaphore du cheval\n", marque);
}
```

### Explications :

**Objectif** : Effectuer le déplacement du cheval de la case de départ vers la case d'arrivée.

**Détails** :
1. **Verrouillage départ** : Si le cheval est déjà sur la piste (depart > 0), on verrouille sa case de départ.
2. **Effacement** : La case de départ est effacée avec `piste_cell_effacer()` qui met le pid à 0.
3. **Libération départ** : On libère immédiatement la case de départ (elle est maintenant libre pour un autre cheval).
4. **Simulation du saut** : Appel à `commun_attendre_fin_saut()` pour simuler le temps de déplacement (le cheval est "en l'air", protégé par son sémaphore).
5. **Atterrissage** : Affectation de la case d'arrivée avec `piste_cell_affecter()`.
6. **Libération arrivée** : La case d'arrivée était verrouillée depuis l'étape 4, on la libère maintenant.
7. **Fin de protection** : Déverrouillage du sémaphore du cheval (il peut maintenant être décanillé à nouveau).

**Note importante** : L'ordre est crucial :
- Case de départ verrouillée → effacée → libérée (rapidement)
- Case d'arrivée verrouillée (depuis étape 4) → remplie → libérée
- Sémaphore du cheval verrouillé pendant tout le déplacement

---

## 6. Suppression du cheval de la liste (ligne 147-150)

### Code à ajouter :

```c
/*
 * Suppression du cheval de la liste
 */

/* Verrouillage de la liste */
struct sembuf op_P_liste_fin = {0, -1, SEM_UNDO};
if (semop(sem_liste, &op_P_liste_fin, 1) == -1) {
    perror("cheval: Erreur verrouillage liste pour suppression");
} else {
    /* Recherche du cheval dans la liste */
    int ind_cheval_fin;
    if (liste_elem_rechercher(&ind_cheval_fin, liste, elem_cheval) == VRAI) {
        /* Suppression du cheval de la liste */
        if (liste_elem_supprimer(liste, ind_cheval_fin) == -1) {
            fprintf(stderr, "cheval '%c': Erreur suppression du cheval de la liste\n", marque);
        }
    } else {
        fprintf(stderr, "cheval '%c': Erreur, cheval non trouvé pour suppression\n", marque);
    }

    /* Libération de la liste */
    struct sembuf op_V_liste_fin = {0, +1, SEM_UNDO};
    if (semop(sem_liste, &op_V_liste_fin, 1) == -1) {
        perror("cheval: Erreur déverrouillage liste pour suppression");
    }
}

/* Destruction du sémaphore individuel du cheval */
if (elem_sem_detruire(&elem_cheval) == -1) {
    fprintf(stderr, "cheval '%c': Erreur destruction du sémaphore du cheval\n", marque);
}

/* Détachement des segments de mémoire partagée */
if (shmdt(liste) == -1) {
    perror("cheval: Erreur détachement liste");
}

if (shmdt(piste) == -1) {
    perror("cheval: Erreur détachement piste");
}
```

### Explications :

**Objectif** : Nettoyer les ressources avant la fin du processus (arrivée ou décanillage).

**Détails** :
1. **Suppression de la liste** :
   - Verrouillage de la liste
   - Recherche du cheval avec `liste_elem_rechercher()`
   - Suppression avec `liste_elem_supprimer()` qui décale tous les éléments suivants
   - Libération de la liste
2. **Destruction du sémaphore** : Appel à `elem_sem_detruire()` qui utilise `semctl()` avec IPC_RMID pour détruire le sémaphore individuel du cheval.
3. **Détachement mémoire** : Utilisation de `shmdt()` pour se détacher des segments de mémoire partagée (liste et piste). Le processus ne pourra plus y accéder.

**Pourquoi c'est important** :
- Libère les ressources système (sémaphores)
- Évite les fuites mémoire
- Maintient la cohérence de la liste (un cheval terminé ne doit plus y figurer)
- Permet à d'autres processus de savoir combien de chevaux sont encore en course

---

## Variables supplémentaires nécessaires

À ajouter au début du `main()`, après les déclarations existantes :

```c
int id_liste;        /* ID du segment de mémoire partagée de la liste */
int id_piste;        /* ID du segment de mémoire partagée de la piste */
int sem_liste;       /* ID du sémaphore protégeant la liste */
int sem_piste;       /* ID de l'ensemble de sémaphores de la piste */
```

---

## Résumé du flux d'exécution

1. **Initialisation** : Le cheval s'enregistre dans la liste et s'attache aux ressources partagées
2. **Boucle de jeu** :
   - Attente de son tour
   - Vérification qu'il n'a pas été décanillé
   - Verrouillage de son sémaphore (protection)
   - Coup de dé
   - Verrouillage de la case d'arrivée
   - Si occupée : décaniller l'occupant
   - Verrouillage case de départ → effacement → libération
   - Saut (attente)
   - Atterrissage (remplissage case d'arrivée) → libération
   - Déverrouillage de son sémaphore
   - Affichage de la piste
3. **Fin** : Suppression de la liste et nettoyage des ressources

---

## Points clés de synchronisation

1. **Liste** : Protégée par un seul sémaphore (accès exclusif à toute la liste)
2. **Piste** : Chaque case protégée par son propre sémaphore (parallélisme possible)
3. **Cheval** : Chaque cheval a son sémaphore pour éviter décanillage pendant déplacement
4. **Ordre de verrouillage** : Toujours respecter le même ordre pour éviter les interblocages :
   - Liste avant piste
   - Sémaphore du cheval avant les cases
   - Case d'arrivée avant case de départ

---

## Conseils de débogage

- Compiler avec `-D_DEBUG_` pour voir les messages de debug
- Vérifier les codes de retour de toutes les fonctions système
- Utiliser `ipcs` pour voir l'état des sémaphores et segments partagés
- Utiliser `ipcrm` pour nettoyer en cas de problème
