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
  int shm_piste_id ;

  int cle_liste ;
  liste_t * liste = NULL ;
  int shm_liste_id ;

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



  while( ! fini )
    {
      /* Attente entre 2 coup de de */
      commun_attendre_tour() ;

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



      /*
       * Avancee sur la piste
       */

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


	} 
      /* Affichage de la piste  */
      piste_afficher_lig( piste );
     
      depart = arrivee ;
    }

  printf( "Le cheval \"%c\" A FRANCHIT LA LIGNE D ARRIVEE\n" , marque );

 
     
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

  exit(0);
}
