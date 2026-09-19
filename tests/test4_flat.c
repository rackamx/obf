/* Flattened by cflatten (C port) - control flow flattened into dispatcher loop */
#include <stdio.h>
void greet ( int n )
{
  int __cf_state = 0 ;
  while (__cf_state != -1)
  {
    switch (__cf_state)
    {
      case 0:
      {
        __cf_state += ((n <= 0)) ? (1) : (2) ;
        break ;
      }
      case 1:
      {
        return ;
      }
      case 2:
      {
        printf ( "hello %d\n" , n ) ;
        return ;
      }
      default:
      {
        __cf_state += -1 - __cf_state ;
        break ;
      }
    }
  }
}

int elsechain ( int x )
{
  int __cf_state = 0 ;
  while (__cf_state != -1)
  {
    switch (__cf_state)
    {
      case 0:
      {
        __cf_state += ((x < 0)) ? (1) : (2) ;
        break ;
      }
      case 1:
      {
        return (- 1) ;
      }
      case 2:
      {
        __cf_state += ((x == 0)) ? (1) : (2) ;
        break ;
      }
      case 3:
      {
        return (0) ;
      }
      case 4:
      {
        __cf_state += ((x < 10)) ? (1) : (2) ;
        break ;
      }
      case 5:
      {
        return (1) ;
      }
      case 6:
      {
        return (2) ;
      }
      default:
      {
        __cf_state += -1 - __cf_state ;
        break ;
      }
    }
  }
}

int logic ( int a , int b )
{
  int __cf_state = 0 ;
  while (__cf_state != -1)
  {
    switch (__cf_state)
    {
      case 0:
      {
        __cf_state += ((a > 0 && b > 0)) ? (1) : (2) ;
        break ;
      }
      case 1:
      {
        return (1) ;
      }
      case 2:
      {
        __cf_state += ((a < 0 || b < 0)) ? (1) : (2) ;
        break ;
      }
      case 3:
      {
        return (- 1) ;
      }
      case 4:
      {
        return (0) ;
      }
      default:
      {
        __cf_state += -1 - __cf_state ;
        break ;
      }
    }
  }
}

int while_true ( )
{
  int i ;

  int __cf_state = 0 ;
  while (__cf_state != -1)
  {
    switch (__cf_state)
    {
      case 0:
      {
        i = 0 ;
        __cf_state += 1 ;
        break ;
      }
      case 1:
      {
        __cf_state += ((1)) ? (1) : (2) ;
        break ;
      }
      case 2:
      {
        i ++ ;
        __cf_state += ((i >= 5)) ? (2) : (3) ;
        break ;
      }
      case 3:
      {
        return (i) ;
      }
      case 4:
      {
        __cf_state += -1 ;
        break ;
      }
      case 5:
      {
        __cf_state += -4 ;
        break ;
      }
      default:
      {
        __cf_state += -1 - __cf_state ;
        break ;
      }
    }
  }
}

int goto_loop ( )
{
  int i ;

  int __cf_state = 0 ;
  while (__cf_state != -1)
  {
    switch (__cf_state)
    {
      case 0:
      {
        i = 0 ;
        __cf_state += 1 ;
        break ;
      }
      case 1:
      {
        i ++ ;
        __cf_state += ((i < 4)) ? (1) : (2) ;
        break ;
      }
      case 2:
      {
        __cf_state += -1 ;
        break ;
      }
      case 3:
      {
        return (i) ;
      }
      default:
      {
        __cf_state += -1 - __cf_state ;
        break ;
      }
    }
  }
}

int for_decl_multi ( )
{
  int s ;
  int i ;
  int j ;

  int __cf_state = 0 ;
  while (__cf_state != -1)
  {
    switch (__cf_state)
    {
      case 0:
      {
        s = 0 ;
        i = 0 ;
        j = 5 ;
        __cf_state += 1 ;
        break ;
      }
      case 1:
      {
        __cf_state += ((i < j)) ? (1) : (3) ;
        break ;
      }
      case 2:
      {
        s += 2 ;
        __cf_state += 1 ;
        break ;
      }
      case 3:
      {
        i ++ , j -- ;
        __cf_state += -2 ;
        break ;
      }
      case 4:
      {
        return (s) ;
      }
      default:
      {
        __cf_state += -1 - __cf_state ;
        break ;
      }
    }
  }
}

int tern ( int x )
{
  int y ;

  int __cf_state = 0 ;
  while (__cf_state != -1)
  {
    switch (__cf_state)
    {
      case 0:
      {
        y = x > 5 ? 100 : 200 ;
        return (y) ;
      }
      default:
      {
        __cf_state += -1 - __cf_state ;
        break ;
      }
    }
  }
}

int main ( )
{
  int __cf_state = 0 ;
  while (__cf_state != -1)
  {
    switch (__cf_state)
    {
      case 0:
      {
        greet ( 0 ) ;
        greet ( 2 ) ;
        printf ( "else -5=%d\n" , elsechain ( - 5 ) ) ;
        printf ( "else 0=%d\n" , elsechain ( 0 ) ) ;
        printf ( "else 5=%d\n" , elsechain ( 5 ) ) ;
        printf ( "else 20=%d\n" , elsechain ( 20 ) ) ;
        printf ( "logic=%d %d %d\n" , logic ( 1 , 2 ) , logic ( - 1 , 2 ) , logic ( 0 , 0 ) ) ;
        printf ( "while_true=%d\n" , while_true ( ) ) ;
        printf ( "goto_loop=%d\n" , goto_loop ( ) ) ;
        printf ( "for_multi=%d\n" , for_decl_multi ( ) ) ;
        printf ( "tern=%d %d\n" , tern ( 3 ) , tern ( 7 ) ) ;
        return (0) ;
      }
      default:
      {
        __cf_state += -1 - __cf_state ;
        break ;
      }
    }
  }
}

