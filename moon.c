#include <stdio.h>
#include <stdlib.h>
#include <conio.h>

int main() {

    char choice;

  while (1) {

    system("cls");
    printf("WELCOME BACK...\n");
    printf("==================\n\n");
    printf("    1. play ZEUS\n");
    printf("    2. run MDPed\n");
    printf("    3. run TMuse\n");
    printf("    4. exit\n");
    printf("==================\n\n");
    printf("PRESS 1, 2, 3, OR 4: ");

    choice = getch();

    if (choice == '1') {

      system("cls");
      system("zeus.exe");

    }

    else if (choice == '2') {

      system("cls");
      system("mdped.exe");

    }

    else if (choice == '3') {

      system("cls");
      system("tmusegui.exe");

    }

    else if (choice == '4') {

      system("cls");
      printf("SEE YOU, SPACE COWBOY...\n");
      break;

    }

  }

  return 0;

}
