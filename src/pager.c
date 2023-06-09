#include "pager.h"
#include "mmu.h"

#include <sys/mman.h>
#include <assert.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define PAGE_SIZE sysconf(_SC_PAGESIZE)
#define NUM_PAGES (UVM_MAXADDR - UVM_BASEADDR + 1) / PAGE_SIZE

typedef struct {
	pid_t pid; //processo alocado
    int numero; //id do quadro
	int pagina; //pagina referenciada
	short usando; // se esta sendo usado
	short referencia; // se foi referenciada
	short aux; // par algoritmo da segunda chance
	short escrito; // se foi escrito
} quadro_t;

typedef struct {
    int numero; //id do bloco
	short usando; // se esta sendo usado
} bloco_t;

typedef struct {
	int tamanho; //tamanho do vetor de quadros e blocos
	quadro_t *quadros;  //vetor de quadros da pagina
	bloco_t *blocos; // vetor de blocos da pagina
} pagina_t;

typedef struct
{
	pid_t pid; // processso alocado a tabela
    int tamanho; //numero de paginas
	pagina_t *paginas; // vetor de paginas
} tabela_t;

int qtd_blocos; // quantidade global de blocos
int qtd_quadros; // quantidade global de quadros
int blocos_livres; // quantidade global de blocos livres
int qtd_tabelas; // quantidade global de tabelas

int clock_aux; // clock auxiliar para algoritmo da segunda chance

bloco_t *blocos; // vetor global de blocos
quadro_t *quadros; // vetor global de quadros
tabela_t *tabelas; // vetor global de tabelas

void inicia_blocos(bloco_t *blocos, int tamanho, int id) {
    for(int i=0; i< tamanho; i++) {
        blocos[i].usando = 0;
        blocos[i].numero = id;
    }
}

void inicia_quadros(quadro_t *quadros, int tamanho, int id) {
	for(int i = 0; i < tamanho; i++) {
		quadros[i].pid = -1;
		quadros[i].pagina = 0;
        quadros[i].numero = id;
		quadros[i].usando = 0;
		quadros[i].referencia = 0;
		quadros[i].aux = 1;
		quadros[i].escrito = 0;
	}
}

void pager_init(int nframes, int nblocks) {
    qtd_tabelas = 1;
    tabelas = (tabela_t*) malloc (sizeof(tabela_t));

    blocos = (bloco_t*) malloc (nblocks * sizeof(bloco_t));
    qtd_blocos = nblocks;
    blocos_livres = nblocks;
	inicia_blocos(blocos, nblocks, 0);

    clock_aux = 0;

    qtd_quadros = nframes;
    quadros = (quadro_t*) malloc (nframes * sizeof(quadro_t));
	inicia_quadros(quadros, nframes, 0);
}

void pager_create(pid_t pid)
{
    int flag = 0;

	for(int i = 0; i < qtd_tabelas; i++)
	{
		if(tabelas[i].paginas == NULL)
		{
			tabelas[i].pid = pid;
			tabelas[i].paginas = (pagina_t*) malloc (sizeof(pagina_t));
			tabelas[i].paginas->tamanho = NUM_PAGES;
			tabelas[i].paginas->quadros = (quadro_t*) malloc (NUM_PAGES * sizeof(quadro_t));
			inicia_quadros(tabelas[i].paginas->quadros, NUM_PAGES, -1);
			tabelas[i].paginas->blocos = (bloco_t*) malloc (NUM_PAGES * sizeof(bloco_t));
			inicia_blocos(tabelas[i].paginas->blocos, NUM_PAGES, -1);
			flag = 1;
			break;
		}
	}

	if(flag == 0)
	{
		tabelas = realloc(tabelas, (100 + qtd_tabelas) * sizeof(tabela_t));
		tabelas[qtd_tabelas].pid = pid;
		tabelas[qtd_tabelas].paginas = (pagina_t*) malloc (sizeof(pagina_t));
		tabelas[qtd_tabelas].paginas->tamanho = NUM_PAGES;
		tabelas[qtd_tabelas].paginas->quadros = (quadro_t*) malloc (NUM_PAGES * sizeof(quadro_t));
		inicia_quadros(tabelas[qtd_tabelas].paginas->quadros, NUM_PAGES, -1);
		tabelas[qtd_tabelas].paginas->blocos = (bloco_t*) malloc (NUM_PAGES * sizeof(bloco_t));
		inicia_blocos(tabelas[qtd_tabelas].paginas->blocos, NUM_PAGES, -1);

		int pos=qtd_tabelas + 1;
		qtd_tabelas += 100;
		for(int j = pos; j < qtd_tabelas; j++)
		{
			tabelas[j].paginas = NULL;
		}
	}
}

void *pager_extend(pid_t pid) {
    if(blocos_livres == 0) return NULL;

	bloco_t bloco;
    int pos=0;
	for(int i = 0; i < qtd_blocos; i++)
	{
		if(blocos[i].usando == 0)
		{
			blocos[i].usando = 1;
            blocos[i].numero = i;
		    blocos_livres--;
			bloco = blocos[i];
			break;
		}
	}

	for(int i = 0; i < qtd_tabelas; i++)
	{
		if(tabelas[i].pid == pid)
		{
			for(int j = 0; j < tabelas[i].paginas->tamanho; j++)
			{
				if(tabelas[i].paginas->blocos[j].numero == -1)
				{
					tabelas[i].paginas->blocos[j] = bloco;
					pos = j;
					break;
				}
				if(j == (tabelas[i].paginas->tamanho) - 1){
					return NULL;
				}
			}
			break;
		}
	}

	return (void*) (UVM_BASEADDR + (intptr_t) (pos * PAGE_SIZE));
}

void pager_fault(pid_t pid, void *vaddr) {
	int pos_pid;
	int mem_no_none;
	void *addr;

	for(int i = 0; i < qtd_tabelas; i++)
	{
		if(tabelas[i].pid == pid)
		{
			pos_pid = i;
			break;
		}
	}

	int pagina = ((((intptr_t) vaddr) - UVM_BASEADDR) / PAGE_SIZE);

	mem_no_none = 1;
	for(int i = 0; i < qtd_quadros; i++)
	{
		if(quadros[i].aux == 1)
		{
			mem_no_none = 0;
			break;
		}
	}
	int quadro_atual;
	if(tabelas[pos_pid].paginas->quadros[pagina].numero != -1 && tabelas[pos_pid].paginas->quadros[pagina].numero != -2)
	{
		quadro_atual = tabelas[pos_pid].paginas->quadros[pagina].numero;
		mmu_chprot(pid, vaddr, PROT_READ | PROT_WRITE);
		quadros[quadro_atual].referencia = 1;
		quadros[quadro_atual].escrito = 1;
	}
	else
	{
		if(mem_no_none)
		{
			for(int i = 0; i < qtd_quadros; i++)
			{
				addr = (void*) (UVM_BASEADDR + (intptr_t) (quadros[i].pagina * PAGE_SIZE));
				mmu_chprot(quadros[i].pid, addr, PROT_NONE);
				quadros[i].aux = 1;
			}
		}
		int pos_pid_disco;
		int novo_quadro = -1;
		while(novo_quadro == -1)
		{
			novo_quadro = -1;
			if(quadros[clock_aux].referencia == 0)
			{
				novo_quadro = clock_aux;
				if(quadros[clock_aux].usando == 1)
				{
					int pid_disco = quadros[clock_aux].pid;
					int pid_pagina_disco = quadros[clock_aux].pagina;
					for(int i = 0; i < qtd_tabelas; i++)
					{
						if(tabelas[i].pid == pid_disco)
						{
							pos_pid_disco = i;
						}
					}

					int bloco_atual = tabelas[pos_pid_disco].paginas->blocos[pid_pagina_disco].numero;
					mmu_nonresident(pid, (void*) (UVM_BASEADDR + (intptr_t) (pid_pagina_disco * PAGE_SIZE)));
					if(quadros[clock_aux].escrito == 1)
					{
						mmu_disk_write(clock_aux, bloco_atual);
						// Marca o frame como vazio e no disco
						tabelas[pos_pid_disco].paginas->quadros[pid_pagina_disco].numero = -2;
					}
					else
					{
						// Marca o frame como vazio
						tabelas[pos_pid_disco].paginas->quadros[pid_pagina_disco].numero = -1;
					}

				}
				quadros[clock_aux].pid = pid;
				quadros[clock_aux].pagina = pagina;
				quadros[clock_aux].usando = 1;
				quadros[clock_aux].referencia = 1;
				quadros[clock_aux].aux = 0;

				if(tabelas[pos_pid].paginas->quadros[pagina].numero == -2)
				{
					int novo_bloco = tabelas[pos_pid].paginas->blocos[pagina].numero;
					mmu_disk_read(novo_bloco, novo_quadro);
					quadros[clock_aux].escrito = 1;
				}
				else
				{
					mmu_zero_fill(novo_quadro);
					quadros[clock_aux].escrito = 0;
				}
				tabelas[pos_pid].paginas->quadros[pagina].numero = novo_quadro;
				mmu_resident(pid, vaddr, novo_quadro, PROT_READ);
			}
			else
			{
				quadros[clock_aux].referencia = 0;
			}
			clock_aux++;
			clock_aux %= qtd_quadros;
		}
	}
}

int pager_syslog(pid_t pid, void *addr, size_t len) {
	int pos_pid;
	char *message = (char*) malloc (len + 1);
	for(int i = 0; i < qtd_tabelas; i++)
	{
		if(tabelas[i].pid == pid)
		{
			pos_pid = i;
			break;
		}
	}
	int quadro;
	for(int i = 0; i < tabelas[pos_pid].paginas->tamanho; i++)
	{
		if(tabelas[pos_pid].paginas->quadros[i].numero == -1)
		{
			quadro = i;
			break;
		}
	}
	int flag;
	for(int i = 0; i < len; i++)
	{
		flag = 1;
		for(int j = 0; j < quadro; j++)
		{
			if(((intptr_t) addr + i - UVM_BASEADDR) / (PAGE_SIZE) == tabelas[pos_pid].paginas->quadros[j].numero)
			{
				flag = 0;
				break;
			}
		}

		if(flag)
			return -1;

		int pag = ((((intptr_t) addr + i)) - UVM_BASEADDR) / PAGE_SIZE;

		int num_quadro = tabelas[pos_pid].paginas->quadros[pag].numero;
		message[i] = pmem[(num_quadro * PAGE_SIZE) + i];
		printf("%02x", (unsigned)message[i]);
		if(i == len-1)
			printf("\n");
	}

	return 0;
}

void pager_destroy(pid_t pid) {
	int i;
	for(i =0; i < qtd_tabelas; i++)
	{
		if(tabelas[i].pid == pid)
		{
			tabelas[i].pid = 0;
			free(tabelas[i].paginas->quadros);
			free(tabelas[i].paginas->blocos);
			free(tabelas[i].paginas);
			tabelas[i].paginas = NULL;
			blocos_livres++;
		}
	}
}