#include <stdio.h>
#include <stdlib.h>
#include <sys/msg.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>

#include <commun.h>
#include <liste.h>
#include <piste.h>

int 
main( int nb_arg , char * tab_arg[] )
{     

  int cle_piste ;
  piste_t * piste = NULL ;

  int cle_liste ;
  /* liste_t * liste = NULL ; */

  char marque ;

  booleen_t fini = FAUX ;
  piste_id_t deplacement = 0 ;
  piste_id_t depart = 0 ;
  piste_id_t arrivee = 0 ;

  
  cell_t cell_cheval ;


  elem_t elem_cheval ;



  /*-----*/

  if( nb_arg != 4 )
    {
      fprintf( stderr, "usage : %s <cle de piste> <cle de liste> <marque>\n" , tab_arg[0] );
      exit(-1);
    }
  
  if( sscanf( tab_arg[1] , "%d" , &cle_piste) != 1 )
    {
      fprintf( stderr, "%s : erreur , mauvaise cle de piste (%s)\n" , 
	       tab_arg[0]  , tab_arg[1] );
      exit(-2);
    }


  if( sscanf( tab_arg[2] , "%d" , &cle_liste) != 1 )
    {
      fprintf( stderr, "%s : erreur , mauvaise cle de liste (%s)\n" , 
	       tab_arg[0]  , tab_arg[2] );
      exit(-2);
    }

  if( sscanf( tab_arg[3] , "%c" , &marque) != 1 )
    {
      fprintf( stderr, "%s : erreur , mauvaise marque de cheval (%s)\n" , 
	       tab_arg[0]  , tab_arg[3] );
      exit(-2);
    }
     

  /* Init de l'attente */
  commun_initialiser_attentes() ;


  /* Init de la cellule du cheval pour faire la course */
  cell_pid_affecter( &cell_cheval  , getpid());
  cell_marque_affecter( &cell_cheval , marque );

  /* Init de l'element du cheval pour l'enregistrement */
  elem_cell_affecter(&elem_cheval , cell_cheval ) ;
  elem_etat_affecter(&elem_cheval , EN_COURSE ) ;

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
  

  while( ! fini )
    {
      /* Attente entre 2 coup de de */
      commun_attendre_tour() ;

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

      /*
       * Avancee sur la piste
       */

      /* Verrouillage du sémaphore du cheval pour éviter d'être décanillé pendant le déplacement */
      if (elem_sem_verrouiller(&elem_cheval) == -1) {
          fprintf(stderr, "cheval '%c': Erreur verrouillage sémaphore du cheval\n", marque);
          fini = VRAI;
          break;
      }

      /* Coup de de */
      deplacement = commun_coup_de_de() ;
#ifdef _DEBUG_
      printf(" Lancement du De --> %d\n", deplacement );
#endif

      arrivee = depart+deplacement ;

      if( arrivee > PISTE_LONGUEUR-1 )
	{
	  arrivee = PISTE_LONGUEUR-1 ;
	  fini = VRAI ;
	}

      if( depart != arrivee )
	{

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

#ifdef _DEBUG_
	  printf("Deplacement du cheval \"%c\" de %d a %d\n",
		 marque, depart, arrivee );
#endif

	  
	} 
      /* Affichage de la piste  */
      piste_afficher_lig( piste );
     
      depart = arrivee ;
    }

  printf( "Le cheval \"%c\" A FRANCHIT LA LIGNE D ARRIVEE\n" , marque );

 
     
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

  
  exit(0);
}
