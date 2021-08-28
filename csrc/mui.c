/*

  mui.c
  
  Monochrome minimal user interface: Core library.

  Universal 8bit Graphics Library (https://github.com/olikraus/u8g2/)

  Copyright (c) 2016, olikraus@gmail.com
  All rights reserved.

  Redistribution and use in source and binary forms, with or without modification, 
  are permitted provided that the following conditions are met:

  * Redistributions of source code must retain the above copyright notice, this list 
    of conditions and the following disclaimer.
    
  * Redistributions in binary form must reproduce the above copyright notice, this 
    list of conditions and the following disclaimer in the documentation and/or other 
    materials provided with the distribution.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND 
  CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, 
  INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE 
  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR 
  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT 
  NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; 
  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER 
  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, 
  STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF 
  ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.  

  

  "mui.c" is a graphical user interface, developed as part of u8g2.
  However "mui.c" is independent of u8g2 and can be used without u8g2 code.
  The glue code between "mui.c" and u8g2 is located in "mui_u8g2.c"

  c: cmd
  i:  ID0
  j: ID1
  xy: Position (x and y)
  /text/: some text. The text can start with any delimiter (except 0 and |), but also has to end with the same delimiter
  a: Single char argument
  u: Single char argument with the user interface form number

  "Uu" the interface                                                    --> no ID
  
  Manual ID:
  "Fijxy"  Generic field: Places field with id ii at x/y        --> ID=ij
  "Bijxy/text/"   Generic field (Button) with Text   --> ID=ij
  "Tiixya/text/"  Generic field with argument and text --> ID = ij
  "Aiixya"
  
  Fixed ID:
  "Si" the style                                                        --> ID=@i
  "Lxy/labeltext/"  Places a text at the specified position, field with   -     -> ID=.L, .l
  "Gxyu/menutext/"  Go to the specified menu without placing the user interface form on the stack       --> ID=.G, .g
  
  
  cijxy
  cijxy/text/
  cijxya/text/
  
  cxy/text/
  cxya/text/
  
*/



#include "mui.h"





//#define mui_get_fds_char(s) ((uint8_t)(*s))


uint8_t mui_get_fds_char(fds_t s)       
{
  return (uint8_t)(*s);
}


/*
  s must point to a valid command within FDS
*/
static size_t mui_fds_get_cmd_size_without_text(mui_t *ui, fds_t s) MUI_NOINLINE;
static size_t mui_fds_get_cmd_size_without_text(mui_t *ui, fds_t s)
{
  uint8_t c = mui_get_fds_char(s);
  c &= 0xdf; /* consider upper and lower case */
  switch(c)
  {
    case 'U': return 2;
    case 'S': return 2;
    case 'F': return 5;         // CMD, ID (2 Bytes), X, Y
    case 'B': return 5;         // CMD, ID (2 Bytes), X, Y, Text (does not count here)
    case 'T': return 6;         // CMD, ID (2 Bytes), X, Y, Arg, Text (does not count here)
    case 'A': return 6;         // CMD, ID (2 Bytes), X, Y, Arg, Text
    case 'L': return 3;
    //case 'M': return 4;
    //case 'X': return 3;
    //case 'J': return 4;
    case 'G': return 4;  
    case 0: return 0;
  }
  return 1;
}


/*
  s must point to the string delimiter start: first '/' for "B00ab/ok/"
    - '/' actually is 0xff
    - return the total size of the string, including the delimiter
    - copies the content of the string ("ok") to the ui text buffer

*/
static size_t mui_fds_parse_text(mui_t *ui, fds_t s)
{
  uint8_t i = 0;
  ui->delimiter = mui_get_fds_char(s);
  uint8_t c;
  fds_t t = s;
  
  //printf("mui_fds_parse_text del=%d\n", delimiter);
#ifdef MUI_CHECK_EOFDS
  if ( ui->delimiter == 0 )
    return 0;
#endif 
  t++;
  for( ;; )
  {
    c = mui_get_fds_char(t);
  //printf("mui_fds_parse_text i=%d, c=%c\n", i, c);
#ifdef MUI_CHECK_EOFDS
    if ( c == 0 )
      break;
#endif 
    if ( c == ui->delimiter )
    {
      t++;
      break;
    }
    if ( i < MUI_MAX_TEXT_LEN )
    {
      ui->text[i++] = c;
    }
    t++;
  }
  ui->text[i] = '\0' ;
  return t-s;
}

/*
  get the first token within a text argument.
  The text argument may look like this:
    "B00ab/banana|apple|peach|cherry/"
  The outer delimiter "/" is not fixed and can be any char except "|" and "\0"
  The inner delimiter "|" is fixed. It must be the pipe symbol.
  This function will place "banana" into ui->text if the result is not 0

  if ( mui_fds_first_token(ui) )
  {
    do 
    {
      // handle token in ui->text
    } while ( mui_fds_next_token(ui) )
  }

*/

uint8_t mui_fds_first_token(mui_t *ui)
{
  ui->token = ui->fds;
  ui->token += mui_fds_get_cmd_size_without_text(ui, ui->fds);
  ui->delimiter = mui_get_fds_char(ui->token);
  ui->token++;  // place ui->token on the first char of the token
  return mui_fds_next_token(ui);
}


uint8_t mui_fds_next_token(mui_t *ui)
{
  uint8_t c;
  uint8_t i = 0;
  // printf("mui_fds_next_token: call, ui->token=%p\n", ui->token);
  for( ;; )
  {
    c = mui_get_fds_char(ui->token);
    // printf("mui_fds_next_token: i=%d c=%c\n", i, c);
#ifdef MUI_CHECK_EOFDS
    if ( c == 0 )
      break;
#endif 
    if ( c == ui->delimiter )
      break;
    if ( c == '|'  )
    {
      ui->token++;  // place ui->token on the first char of the next token
      break;
    }
    
    if ( i < MUI_MAX_TEXT_LEN )
    {
      ui->text[i++] = c;
    }
    
    ui->token++;
  }
  ui->text[i] = '\0' ;
  if ( i == 0 )
    return 0;   // no further token found
  return 1;  // token placed in ui->text
}

/*
  find nth token, return 0 if n exceeds the number of tokens, 1 otherwise
  the result is stored in ui->text
*/
uint8_t mui_fds_get_nth_token(mui_t *ui, uint8_t n)
{  
  // printf("mui_fds_get_nth_token: call, n=%d\n", n);
  if ( mui_fds_first_token(ui) )
  {
    do 
    {
      if ( n == 0 )
      {
        // printf("mui_fds_get_nth_token: found");
        return 1;
      }
      n--;
    } while ( mui_fds_next_token(ui) );
  }
  //printf("mui_fds_get_nth_token: NOT found\n");
  return 0;
}

uint8_t mui_fds_get_token_cnt(mui_t *ui)
{
  uint8_t n = 0;
  if ( mui_fds_first_token(ui) )
  {
    do 
    {
      n++;
    } while ( mui_fds_next_token(ui) );
  }
  return n;
}


#define mui_fds_is_text(c) ( (c) == 'U' || (c) == 'S' || (c) == 'F' || (c) == 'A' ? 0 : 1 )

/*
  s must point to a valid command within FDS
  return
    The complete length of the command (including any text part)
  sideeffect:
    Any existing text part will be copied into ui->text
    ui->text will be assigned to empty string if there is no text argument
*/
static size_t mui_fds_get_cmd_size(mui_t *ui, fds_t s) MUI_NOINLINE;
static size_t mui_fds_get_cmd_size(mui_t *ui, fds_t s)
{
  size_t l = mui_fds_get_cmd_size_without_text(ui, s);
  uint8_t c = mui_get_fds_char(s);
 ui->text[0] = '\0' ;   /* always reset the text buffer */
 if ( mui_fds_is_text(c) )
  {
    l += mui_fds_parse_text(ui, s+l);
  }
  return l;
}



void mui_Init(mui_t *ui, void *graphics_data, fds_t fds, muif_t *muif_tlist, size_t muif_tcnt)
{
  memset(ui, 0, sizeof(mui_t));
  ui->root_fds = fds;
  ui->current_form_fds = fds;
  ui->muif_tlist = muif_tlist;
  ui->muif_tcnt = muif_tcnt;
  ui->graphics_data = graphics_data;
}

ssize_t mui_find_uif(mui_t *ui, uint8_t id0, uint8_t id1)
{
  ssize_t i;
  for( i = 0; i < ui->muif_tcnt; i++ )
  {
      if ( ui->muif_tlist[i].id0 == id0 )
        if ( ui->muif_tlist[i].id1 == id1 )
          return i;
  }
  return -1;
}


/*
  assumes a valid position in ui->fds and calculates all the other variables
  some fields are alway calculated like the ui->cmd and ui->len field
  other member vars are calculated only if the return value is 1
  will return 1 if the field id was found.
  will return 0 if the field id was not found in uif or if ui->fds points to something else than a field
*/
uint8_t mui_prepare_current_field(mui_t *ui)
{
  ssize_t muif_tidx;

  ui->uif = NULL;
  ui->dflags = 0;    
  ui->id0 = 0;
  ui->id1 = 0;
  ui->arg = 0;

  /* calculate the length of the command and copy the text argument */
  /* this will also clear the text in cases where there is no text argument */
  ui->len = mui_fds_get_cmd_size(ui, ui->fds); 

  /* get the command and check whether end of form is reached */
  ui->cmd = mui_get_fds_char(ui->fds);
  
  /* Copy the cmd also to second id value. This is required for some commands, others will overwrite this below */
  ui->id1 = ui->cmd;
  
  /* now make the command uppercase so that both, upper and lower case are considered */
  ui->cmd &= 0xdf; /* consider upper and lower case */
  
  if ( ui->cmd == 'U' || ui->cmd == 0 )
    return 0;

  /* calculate the dynamic flags */
  if ( ui->fds == ui->cursor_focus_fds )
    ui->dflags |= MUIF_DFLAG_IS_CURSOR_FOCUS;
  if ( ui->fds == ui->touch_focus_fds )
    ui->dflags |= MUIF_DFLAG_IS_TOUCH_FOCUS;
  

  /* get the id0 and id1 values */
  if  ( ui->cmd == 'F' || ui->cmd == 'B' || ui->cmd == 'T' || ui->cmd == 'A' )
  {
      ui->id0 = mui_get_fds_char(ui->fds+1);
      ui->id1 = mui_get_fds_char(ui->fds+2);
      ui->x = mui_get_fds_char(ui->fds+3);
      ui->y = mui_get_fds_char(ui->fds+4);
      if ( ui->cmd == 'A' || ui->cmd == 'T' )
      {
        ui->arg = mui_get_fds_char(ui->fds+5);
      }
  }
  else if ( ui->cmd == 'S' )
  {
      ui->id0 = '@';
      ui->id1 = mui_get_fds_char(ui->fds+1);
  }
  else
  {
      ui->id0 = '.';
      /* note that ui->id1 contains the original cmd value */
      ui->x = mui_get_fds_char(ui->fds+1);
      ui->y = mui_get_fds_char(ui->fds+2);
      if ( ui->cmd == 'G' || ui->cmd == 'M' )  /* this is also true for 'g' or 'm' */
      {
        ui->arg = mui_get_fds_char(ui->fds+3);
      }
  }
  
  /* find the field  */
  muif_tidx = mui_find_uif(ui, ui->id0, ui->id1);
  //printf("mui_prepare_current_field: muif_tidx=%d\n", muif_tidx);
  if ( muif_tidx >= 0 )
  {
    ui->uif = ui->muif_tlist + muif_tidx;
    return 1;
  }
  return 0;
}

/* 
  assumes that ui->fds has been assigned correctly 
  and that ui->target_fds and ui->tmp_fds had been cleared if required

  Usually do not call this function directly, instead use mui_loop_over_form

*/

void mui_inner_loop_over_form(mui_t *ui, uint8_t (*task)(mui_t *ui)) MUI_NOINLINE;
void mui_inner_loop_over_form(mui_t *ui, uint8_t (*task)(mui_t *ui))
{
  uint8_t cmd;
  
  ui->fds += mui_fds_get_cmd_size(ui, ui->fds);      // skip the first entry, it is U always
  for(;;)
  {    
    //printf("fds=%p *fds=%d\n", ui->fds, ui->fds[0]);
    /* get the command and check whether end of form is reached */
    cmd = mui_get_fds_char(ui->fds);
    if ( cmd == 'U' || cmd == 0 )
      break;
    if ( mui_prepare_current_field(ui) )
      if ( task(ui) )         /* call the task, which was provided as argument to this function */
        break;
    ui->fds += ui->len;
  }
  //printf("mui_loop_over_form ended\n");
}

void mui_loop_over_form(mui_t *ui, uint8_t (*task)(mui_t *ui))
{
  if ( ui->current_form_fds == NULL )
    return;
  
  ui->fds = ui->current_form_fds;
  ui->target_fds = NULL;
  ui->tmp_fds = NULL;
  
  mui_inner_loop_over_form(ui, task);  
}

/*
  n is the form number
*/
fds_t mui_find_form(mui_t *ui, uint8_t n)
{
  fds_t fds = ui->root_fds;
  uint8_t cmd;
  
  for( ;; )
  {
    cmd = mui_get_fds_char(fds);
    if ( cmd == 0 )
      break;
    if ( cmd == 'U'  )
    {
      if (   mui_get_fds_char(fds+1) == n )
      {
        return fds;
      }
      /* not found, just coninue */
    }
    
    fds += mui_fds_get_cmd_size(ui, fds);
  }
  return NULL;
}

/* === task procedures (arguments for mui_loop_over_form) === */
/* ui->fds contains the current field */

uint8_t mui_task_draw(mui_t *ui)
{
  //printf("mui_task_draw fds=%p uif=%p text=%s\n", ui->fds, ui->uif, ui->text);
  muif_get_cb(ui->uif)(ui, MUIF_MSG_DRAW);
  return 0;     /* continue with the loop */
}

uint8_t mui_task_form_start(mui_t *ui)
{
  muif_get_cb(ui->uif)(ui, MUIF_MSG_FORM_START);
  return 0;     /* continue with the loop */
}

uint8_t mui_task_form_end(mui_t *ui)
{
  muif_get_cb(ui->uif)(ui, MUIF_MSG_FORM_END);
  return 0;     /* continue with the loop */
}


uint8_t mui_task_find_prev_cursor_uif(mui_t *ui)
{
  if ( muif_get_cflags(ui->uif) & MUIF_CFLAG_IS_CURSOR_SELECTABLE )
  {
    if ( ui->fds == ui->cursor_focus_fds )
    {
      ui->target_fds = ui->tmp_fds;
      return 1;         /* stop looping */
    }
    ui->tmp_fds = ui->fds;
  }
  return 0;     /* continue with the loop */
}

uint8_t mui_task_find_first_cursor_uif(mui_t *ui)
{
  if ( muif_get_cflags(ui->uif) & MUIF_CFLAG_IS_CURSOR_SELECTABLE )
  {
    // if ( ui->target_fds == NULL )
    // {
      ui->target_fds = ui->fds;
      return 1;         /* stop looping */
    // }
  }
  return 0;     /* continue with the loop */
}

uint8_t mui_task_find_last_cursor_uif(mui_t *ui)
{
  if ( muif_get_cflags(ui->uif) & MUIF_CFLAG_IS_CURSOR_SELECTABLE )
  {
    //ui->cursor_focus_position++;
    ui->target_fds = ui->fds;
  }
  return 0;     /* continue with the loop */
}

uint8_t mui_task_find_next_cursor_uif(mui_t *ui)
{
  if ( muif_get_cflags(ui->uif) & MUIF_CFLAG_IS_CURSOR_SELECTABLE )
  {
    if ( ui->tmp_fds != NULL )
    {
      ui->target_fds = ui->fds;        
      ui->tmp_fds = NULL;
      return 1;         /* stop looping */
    }
    if ( ui->fds == ui->cursor_focus_fds )
    {
      ui->tmp_fds = ui->fds;
    }
  }
  return 0;     /* continue with the loop */
}

uint8_t mui_task_get_current_cursor_focus_position(mui_t *ui)
{
  if ( muif_get_cflags(ui->uif) & MUIF_CFLAG_IS_CURSOR_SELECTABLE )
  {
    if ( ui->fds == ui->cursor_focus_fds )
      return 1;         /* stop looping */
    ui->tmp8++;
  }
  return 0;     /* continue with the loop */
}

uint8_t mui_task_read_nth_seleectable_field(mui_t *ui)
{
  if ( muif_get_cflags(ui->uif) & MUIF_CFLAG_IS_CURSOR_SELECTABLE )
  {
    if ( ui->tmp8 == 0 )
      return 1;         /* stop looping */
    ui->tmp8--;
  }
  return 0;     /* continue with the loop */
}


/* === utility functions for the user API === */

void mui_send_cursor_msg(mui_t *ui, uint8_t msg)
{
  if ( ui->cursor_focus_fds )
  {
    ui->fds = ui->cursor_focus_fds;
    if ( mui_prepare_current_field(ui) )
      muif_get_cb(ui->uif)(ui, msg);
  }
}

/* === user API === */

/* 
  returns the field pos which has the current focus 
  If the first selectable field has the focus, then 0 will be returned
  Unselectable fields (for example labels) are skipped by this count.
  If no fields are selectable, then 0 is returned
*/
uint8_t mui_GetCurrentCursorFocusPosition(mui_t *ui)
{
  ui->tmp8 = 0;
  mui_loop_over_form(ui, mui_task_get_current_cursor_focus_position);
  return ui->tmp8;
}


void mui_Draw(mui_t *ui)
{
  mui_loop_over_form(ui, mui_task_draw);
}

void mui_next_field(mui_t *ui)
{
  mui_loop_over_form(ui, mui_task_find_next_cursor_uif);
  // ui->cursor_focus_position++;
  ui->cursor_focus_fds = ui->target_fds;      // NULL is ok  
  if ( ui->target_fds == NULL )
  {
    mui_loop_over_form(ui, mui_task_find_first_cursor_uif);
    ui->cursor_focus_fds = ui->target_fds;      // NULL is ok  
    // ui->cursor_focus_position = 0;
  }
}

/*
  this function will overwrite the ui field related member variables
  nth_token can be 0 if the fiel text is not a option list
  the result is stored in ui->text
*/
void mui_GetSelectableFieldTextOption(mui_t *ui, uint8_t form_id, uint8_t cursor_position, uint8_t nth_token)
{
  fds_t fds = ui->fds;                                // backup the current fds, so that this function can be called inside a task loop 
  ssize_t len = ui->len;          // backup length of the current command

  
  ui->fds = mui_find_form(ui, form_id);          // search for the target form and overwrite the current fds

  // use the inner_loop procedure, because ui->fds has been assigned already
  ui->tmp8 = cursor_position;   // maybe we should also backup tmp8, but at the moment tmp8 is only used by mui_task_get_current_cursor_focus_position
  mui_inner_loop_over_form(ui, mui_task_read_nth_seleectable_field);
  // at this point ui->fds contains the field which was selected from above
  
  // now get the opion string out of the text field. nth_token can be 0 if this is no opion string
  mui_fds_get_nth_token(ui, nth_token);          // return value is ignored here
  
  ui->fds = fds;                        // restore the previous fds position
  ui->len = len;
  // result is stored in ui->text
}

/* 
  input: current_form_fds 
  if called from a field function, then the current field variables are destroyed, so that call should be the last call in the field callback.
*/
void mui_EnterForm(mui_t *ui, uint8_t initial_cursor_position)
{
  /* clean focus fields */
  ui->touch_focus_fds = NULL;
  ui->cursor_focus_fds = NULL;
  
  /* inform all fields that we start a new form */
  mui_loop_over_form(ui, mui_task_form_start);
  
  /* assign initional cursor focus */
  mui_loop_over_form(ui, mui_task_find_first_cursor_uif);  
  ui->cursor_focus_fds = ui->target_fds;      // NULL is ok  
  
  while( initial_cursor_position > 0 )
  {
    mui_next_field(ui);
    initial_cursor_position--;
  }
  
  mui_send_cursor_msg(ui, MUIF_MSG_CURSOR_ENTER);
}

/* input: current_form_fds */
/*
  if called from a field function, then the current field variables are destroyed, so that call should be the last call in the field callback.
*/
void mui_LeaveForm(mui_t *ui)
{
  mui_send_cursor_msg(ui, MUIF_MSG_CURSOR_LEAVE);
  ui->cursor_focus_fds = NULL;
  
  /* inform all fields that we leave the form */
  mui_loop_over_form(ui, mui_task_form_end);  
  ui->current_form_fds = NULL;
}

/* 0: error, form not found */
/*
  if called from a field function, then the current field variables are destroyed, so that call should be the last call in the field callback.
*/
uint8_t mui_GotoForm(mui_t *ui, uint8_t form_id, uint8_t initial_cursor_position)
{
  fds_t fds = mui_find_form(ui, form_id);
  if ( fds == NULL )
    return 0;
  mui_LeaveForm(ui);
  ui->current_form_fds = fds;
  mui_EnterForm(ui, initial_cursor_position);
  return 1;
}

void mui_SaveForm(mui_t *ui)
{
  if ( ui->current_form_fds == NULL )
    return;

  ui->last_form_id = mui_get_fds_char(ui->current_form_fds+1);
  ui->last_form_cursor_focus_position = mui_GetCurrentCursorFocusPosition(ui);
}

/*
  if called from a field function, then the current field variables are destroyed, so that call should be the last call in the field callback.
*/
void mui_RestoreForm(mui_t *ui)
{
  mui_GotoForm(ui, ui->last_form_id, ui->last_form_cursor_focus_position);
}

/*
  updates "ui->cursor_focus_fds"
*/
/*
  if called from a field function, then the current field variables are destroyed, so that call should be the last call in the field callback.
*/
void mui_NextField(mui_t *ui)
{
  mui_send_cursor_msg(ui, MUIF_MSG_CURSOR_LEAVE);
  mui_next_field(ui);
  mui_send_cursor_msg(ui, MUIF_MSG_CURSOR_ENTER);
}

/*
  updates "ui->cursor_focus_fds"
*/
/*
  if called from a field function, then the current field variables are destroyed, so that call should be the last call in the field callback.
*/
void mui_PrevField(mui_t *ui)
{
  mui_send_cursor_msg(ui, MUIF_MSG_CURSOR_LEAVE);
  
  mui_loop_over_form(ui, mui_task_find_prev_cursor_uif);
  //ui->cursor_focus_position--;
  ui->cursor_focus_fds = ui->target_fds;      // NULL is ok  
  if ( ui->target_fds == NULL )
  {
    //ui->cursor_focus_position = 0;
    mui_loop_over_form(ui, mui_task_find_last_cursor_uif);
    ui->cursor_focus_fds = ui->target_fds;      // NULL is ok  
  }
  
  mui_send_cursor_msg(ui, MUIF_MSG_CURSOR_ENTER);
}


void mui_SendSelect(mui_t *ui)
{
  mui_send_cursor_msg(ui, MUIF_MSG_CURSOR_SELECT);  
}

