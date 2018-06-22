#ifndef PS_DEVELOPER_SETUP_H
#define PS_DEVELOPER_SETUP_H

/* Nonzero for normal production startup; the rest of this file will be ignored.
 */
#define PS_PRODUCTION_STARTUP 0

/* Declare the heroes you want. This also establishes the party size.
 * Available: SWORDSMAN, ARCHER, GADGETEER, NURSE, WIZARD, VAMPIRE, MARTYR, IMMORTAL, BOMBER, BALA
 */
#define PS_TEST_GAME_PARTY IMMORTAL,BOMBER,VAMPIRE

/* Declare the scenario generator mode.
 * One of: NORMAL, SELECTED, ALL
 */
#define PS_TEST_GENERATOR_MODE SELECTED

/* Parameters for NORMAL and SELECTED modes.
 */
#define PS_TEST_DIFFICULTY 1
#define PS_TEST_LENGTH     2

/* Parameters for SELECTED mode.
 * Must name at least one blueprint with a HERO POI.
 * Region may be <0 to select randomly.
 */
#define PS_TEST_BLUEPRINTS  2,192
#define PS_TEST_REGION     -1

/* Parameters for ALL mode.
 * (LO>HI) means extend to end of list.
 */
#define PS_TEST_BLUEPRINTID_LO    130
#define PS_TEST_BLUEPRINTID_HI    -1

/*----------------- Routine use ends here. Permanent config below. -----------*/

/* Setup test game.
 * This must return >0 if a test game was configured, 0 to proceed with normal startup, or <0 for errors.
 */

static int ps_setup_test_game(struct ps_game *game,struct ps_userconfig *config) {

  if (PS_PRODUCTION_STARTUP) return 0;

  /* Set up party.
   */
  const int SWORDSMAN=1,ARCHER=2,GADGETEER=3,NURSE=4,WIZARD=5,VAMPIRE=6,MARTYR=7,IMMORTAL=8,BOMBER=9,BALA=10;
  int plrdefidv[]={PS_TEST_GAME_PARTY};
  int playerc=sizeof(plrdefidv)/sizeof(int);
  if (ps_game_set_player_count(game,playerc)<0) return -1;
  int i=0; for (;i<playerc;i++) {
    if (ps_game_configure_player(game,1+i,plrdefidv[i],0,0)<0) return -1;
  }
  if (ps_input_set_noninteractive_device_assignment()<0) return -1;
  
  if (ps_game_set_difficulty(game,PS_TEST_DIFFICULTY)<0) return -1;
  if (ps_game_set_length(game,PS_TEST_LENGTH)<0) return -1;

  const int NORMAL=1,SELECTED=2,ALL=3;
  switch (PS_TEST_GENERATOR_MODE) {
  
    case 1: { /* Normal scenario generation. */
        if (ps_game_generate(game)<0) return -1;
      } break;

    case 2: { /* Generate with selected blueprints. */
        if (ps_game_generate_test(game,PS_TEST_REGION,PS_TEST_BLUEPRINTS)<0) return -1;
      } break;

    case 3: { /* Generate a scenario with every blueprint. */
        if (ps_game_generate_all_blueprints_test(game,PS_TEST_BLUEPRINTID_LO,PS_TEST_BLUEPRINTID_HI)<0) return -1;
      } break;

    default: {
        ps_log(GAME,ERROR,"Unexpected value %d for PS_TEST_GENERATOR_MODE.",PS_TEST_GENERATOR_MODE);
        return -1;
      }
  }

  if (ps_game_restart(game)<0) return -1;

  return 1;
}

#endif
