#include "pager.h"
#include "mmu.h"
#include "uvm.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <pthread.h>
#include <stdint.h>

typedef struct Quadro {
    pid_t pid;
    int pagina;
    int numero;
    int livre;
    int acessado;
    int escrito;
    int none;
} quadro_t;

typedef struct Bloco {
    int numero;
    int livre;
} bloco_t;

typedef struct ListaBloco {
    int tamanho;
    int qtd_livres;
    bloco_t *blocos;
} lista_blocos_t;

typedef struct ListaQuadro {
    int tamanho;
    quadro_t *quadros;
} lista_quadros_t;

typedef struct Pagina {
    int numero;
    int tamanho;
    lista_blocos_t lista_blocos;
    lista_quadros_t lista_quadros;
} pagina_t;

typedef struct TabelaPaginas {
    pid_t pid;
    int tamanho;
    pagina_t *pagina;
} tabela_paginas_t;


lista_blocos_t lista_blocos;
lista_quadros_t lista_quadros;
tabela_paginas_t *lista_tabela_paginas;

int clock_ptr;
pthread_mutex_t mutex;

void blocks_init(bloco_t *blocos) {
    for (int i = 0; i < lista_blocos.tamanho; i++) {
        blocos[i].numero = i;
        blocos[i].livre = 1;
    }
}

void frames_init(quadro_t *quadros) {
    for (int i = 0; i < lista_quadros.tamanho; i++) {
        quadros[i].numero = i;
        quadros[i].livre = 1;
        quadros[i].pid = -1;
        quadros[i].acessado = 0;
        quadros[i].escrito = 0;
        quadros[i].none = 1;
    }
}

void pager_init(int nframes, int nblocks) {
    
    lista_tabela_paginas = (tabela_paginas_t *) malloc(sizeof(tabela_paginas_t));
    lista_tabela_paginas->tamanho = 1;

    lista_blocos.tamanho = nblocks;
    lista_blocos.qtd_livres = nblocks;
    lista_blocos.blocos = (bloco_t *) malloc(sizeof(bloco_t) * nblocks);
    blocks_init(lista_blocos.blocos);

    lista_quadros.tamanho = nframes;
    lista_quadros.quadros = (quadro_t *) malloc(sizeof(quadro_t) * nframes);
    frames_init(lista_quadros.quadros);

    clock_ptr = 0;
}

void pager_create(pid_t pid) {
    //pthread_mutex_lock(&mutex);

    int num_pages = (UVM_MAXADDR - UVM_BASEADDR + 1) / sysconf(_SC_PAGESIZE);

    int pos = 0;
    int flag = 0;

    for (int i = 0; i < lista_tabela_paginas->tamanho; i++) {
        if (lista_tabela_paginas[i].pagina == NULL) {
            lista_tabela_paginas[i].pid = pid;
            lista_tabela_paginas[i].pagina = (pagina_t*) malloc(sizeof(pagina_t));
            lista_tabela_paginas[i].pagina->tamanho = num_pages;

            lista_tabela_paginas[i].pagina->lista_blocos.tamanho = num_pages;
            lista_tabela_paginas[i].pagina->lista_blocos.blocos = (bloco_t*) malloc(sizeof(bloco_t) * num_pages);
            blocks_init(lista_tabela_paginas[i].pagina->lista_blocos.blocos);

            lista_tabela_paginas[i].pagina->lista_quadros.tamanho = num_pages;
            lista_tabela_paginas[i].pagina->lista_quadros.quadros = (quadro_t*) malloc(sizeof(quadro_t) * num_pages);
            frames_init(lista_tabela_paginas[i].pagina->lista_quadros.quadros);

            flag=1;
            break;
        }
    }

    if (!flag)
    {
        lista_tabela_paginas = realloc(lista_tabela_paginas, (lista_tabela_paginas->tamanho + 100) * sizeof(tabela_paginas_t));
        lista_tabela_paginas[lista_tabela_paginas->tamanho].pid = pid;
        lista_tabela_paginas[lista_tabela_paginas->tamanho].pagina = (pagina_t*) malloc(sizeof(pagina_t));
        lista_tabela_paginas[lista_tabela_paginas->tamanho].pagina->tamanho = num_pages;
        
        lista_tabela_paginas[lista_tabela_paginas->tamanho].pagina->lista_blocos.tamanho = num_pages;
        lista_tabela_paginas[lista_tabela_paginas->tamanho].pagina->lista_blocos.blocos = (bloco_t*) malloc(sizeof(bloco_t) * num_pages);
        lista_tabela_paginas[lista_tabela_paginas->tamanho].pagina->lista_blocos.qtd_livres = num_pages;
        blocks_init(lista_tabela_paginas[lista_tabela_paginas->tamanho].pagina->lista_blocos.blocos);

        lista_tabela_paginas[lista_tabela_paginas->tamanho].pagina->lista_quadros.tamanho = num_pages;
        lista_tabela_paginas[lista_tabela_paginas->tamanho].pagina->lista_quadros.quadros = (quadro_t*) malloc(sizeof(quadro_t) * num_pages);
        frames_init(lista_tabela_paginas[lista_tabela_paginas->tamanho].pagina->lista_quadros.quadros);

        pos = lista_tabela_paginas->tamanho + 1;
        lista_tabela_paginas->tamanho += 100;
        for (int j=pos; j < lista_tabela_paginas->tamanho; j++)
        {
            lista_tabela_paginas->pagina = NULL;
        }
        
    }
    //pthread_mutex_unlock(&mutex);

}

void *pager_extend(pid_t pid) {
    //pthread_mutex_lock(&mutex);

    if (lista_blocos.qtd_livres == 0) return NULL;

    bloco_t bloco;
    int pos = 0;
 
    for (int i=0; i < lista_blocos.tamanho; i++) {
        if (lista_blocos.blocos[i].livre) {
            lista_blocos.blocos[i].livre = 0;
            lista_blocos.qtd_livres -= 1;
            bloco = lista_blocos.blocos[i];
            break;
        }
    }
    for (int i=0; i<lista_tabela_paginas->tamanho; i++) {
        if (lista_tabela_paginas[i].pid == pid)
        {
            for (int j=0; j < lista_tabela_paginas[i].pagina->lista_blocos.tamanho; j++) {
                if (lista_tabela_paginas[i].pagina->lista_blocos.blocos[j].livre) {
                    lista_tabela_paginas[i].pagina->lista_blocos.blocos[j] = bloco;
                    lista_tabela_paginas[i].pagina->lista_blocos.qtd_livres -= 1;
                    pos = j;
                    break;
                }
                if (pos == (lista_tabela_paginas[i].pagina->tamanho - 1)) return NULL;
            }
            break;
        }
    }
    
    //pthread_mutex_unlock(&mutex);
    return (void*) (UVM_BASEADDR + (intptr_t) (pos * sysconf(_SC_PAGESIZE)));
}

int pager_syslog(pid_t pid, void *addr, size_t len) {
    //pthread_mutex_lock(&mutex);
    int indice = 0;
    int limite = 0;
    int flag = 0;
    int pos = 0;

    char *message = (char*) malloc (len + 1);

    for(int i=0;i<lista_tabela_paginas->tamanho;i++) {
        if (lista_tabela_paginas[i].pid == pid)
        {
            indice = i;
            break;
        }
    }

    for (int i=0;i<lista_tabela_paginas[indice].pagina->lista_quadros.tamanho;i++) {
        if(lista_tabela_paginas[indice].pagina->lista_quadros.quadros[i].livre)
        {
            limite = i;
            break;
        }
    }

    for (int i=0; i<len; i++) {
        flag = 1;
        for (int j=0; j<limite; j++) {
            if (lista_tabela_paginas[indice].pagina->lista_quadros.quadros[j].numero == ((intptr_t) addr + i - UVM_BASEADDR) / (sysconf(_SC_PAGESIZE)))
            {
                flag = 0;
                pos = j;
                break;
            }
        }
        if (flag) return -1;

        int pagina = lista_tabela_paginas[indice].pagina->lista_quadros.quadros[pos].numero;
        int indice_quadro = lista_tabela_paginas[indice].pagina->lista_quadros.quadros[pagina].numero;
        message[i] = pmem[(indice_quadro * sysconf(_SC_PAGESIZE)) + i];
        printf("%02x", (unsigned)message[i]);
		if(i == len-1) printf("\n");
    }
    //pthread_mutex_unlock(&mutex);

    return 0;
}

void pager_fault(pid_t pid, void *vaddr) {
    pthread_mutex_lock(&mutex);
    int i, index, index2, page_num, new_frame, new_block,
		move_disk_pid, move_disk_pnum, mem_no_none;
	void *addr;

    int curr_frame;
    int curr_block;

	// Procura o índice da tabela de página do processo pid na lista de tabelas.
	for(i = 0; i < lista_tabela_paginas->tamanho; i++)
	{
		if(lista_tabela_paginas[i].pid == pid)
		{
			// Salva o indice e sai.
			index = i;
			break;
		}
	}

	// Pega o número do quadro na tabela do processo.
	page_num = ((((intptr_t) vaddr) - UVM_BASEADDR) / (sysconf(_SC_PAGESIZE)));

	mem_no_none = 1;
	for(i = 0; i < lista_quadros.tamanho; i++)
	{
		if(lista_quadros.quadros[i].none == 1)
		{
			mem_no_none = 0;
			break;
		}
	}
	// Se esse quadro está carregado.
	if(!lista_tabela_paginas[index].pagina->lista_quadros.quadros[page_num].livre)
	{
		// Salva o índice do vetor de quadros (memória).
		curr_frame = lista_tabela_paginas[index].pagina->lista_quadros.quadros[page_num].numero;
		// Dá permissão de escrita para o processo pid.
		mmu_chprot(pid, vaddr, PROT_READ | PROT_WRITE);
		// Marca o bit de referência no vetor de quadros (memória).
		lista_quadros.quadros[curr_frame].acessado = 1;
		//marca que houve escrita
		lista_quadros.quadros[curr_frame].escrito = 1;
	}
	else // Se não está carregado:
	{
		if(mem_no_none)
		{
			for(i = 0; i < lista_quadros.tamanho; i++)
			{
				addr = (void*) (UVM_BASEADDR + (intptr_t) (lista_quadros.quadros[i].pagina * sysconf(_SC_PAGESIZE)));
				mmu_chprot(lista_quadros.quadros[i].pid, addr, PROT_NONE);
				lista_quadros.quadros[i].none = 1;
			}
		}
		new_frame = -1;
		while(new_frame == -1)
		{
			new_frame = -1;
			// Se o bit de referência é zero.
			if(!lista_quadros.quadros[clock_ptr].acessado)
			{
				new_frame = clock_ptr;
				// Se o frame está em uso.
				if(!lista_quadros.quadros[clock_ptr].livre)
				{
					// Remove o frame e Salva o frame no disco se tiver permissão de escrita.
					move_disk_pid = lista_quadros.quadros[clock_ptr].pid;
					move_disk_pnum = lista_quadros.quadros[clock_ptr].pagina;
					for(i = 0; i < lista_tabela_paginas->tamanho; i++)
					{
						if(lista_tabela_paginas[i].pid == move_disk_pid)
						{
							index2 = i;
						}
					}

					curr_block = lista_tabela_paginas[index2].pagina->lista_blocos.blocos[move_disk_pnum].numero;
					mmu_nonresident(pid, (void*) (UVM_BASEADDR + (intptr_t) (move_disk_pnum * sysconf(_SC_PAGESIZE))));
					if(lista_quadros.quadros[clock_ptr].escrito)
					{
						mmu_disk_write(clock_ptr, curr_block);
						// Marca o frame como vazio (sem uso) que está no disco
						lista_tabela_paginas[index2].pagina->lista_quadros.quadros[move_disk_pnum].livre = 2;
					}
					else
					{
						// Marca o frame como vazio (sem uso).
						lista_tabela_paginas[index2].pagina->lista_quadros.quadros[move_disk_pnum].livre = 1;
					}

				}
				// Coloca o novo processo no vetor de quadros.
				lista_quadros.quadros[clock_ptr].pid = pid;
				lista_quadros.quadros[clock_ptr].pagina = page_num;
				lista_quadros.quadros[clock_ptr].livre = 0;
				lista_quadros.quadros[clock_ptr].acessado = 1;
				lista_quadros.quadros[clock_ptr].none = 0;
				if(lista_tabela_paginas[index].pagina->lista_quadros.quadros[page_num].livre == 2)
				{
					new_block = lista_tabela_paginas[index].pagina->lista_blocos.blocos[page_num].numero;
					mmu_disk_read(new_block, new_frame);
					lista_quadros.quadros[clock_ptr].escrito = 1;
				}
				else
				{
					mmu_zero_fill(new_frame);
					lista_quadros.quadros[clock_ptr].escrito = 0;
				}
				lista_tabela_paginas[index].pagina->lista_quadros.quadros[page_num].numero = new_frame;
				mmu_resident(pid, vaddr, new_frame, PROT_READ);
			}
			else lista_quadros.quadros[clock_ptr].acessado = 0;

			clock_ptr++;
			clock_ptr %= lista_quadros.tamanho;
		}
	}
    //pthread_mutex_unlock(&mutex);
}

void pager_destroy(pid_t pid) {
    //pthread_mutex_lock(&mutex);
    for (int i=0;i<lista_tabela_paginas->tamanho;i++) {
        if (lista_tabela_paginas[i].pid == pid)
        {
            lista_tabela_paginas[i].pid = 0;
            free(lista_tabela_paginas[i].pagina->lista_blocos.blocos);
            free(lista_tabela_paginas[i].pagina->lista_quadros.quadros);
            free(lista_tabela_paginas[i].pagina);
            lista_tabela_paginas[i].pagina = NULL;
            lista_blocos.qtd_livres +=1;
        }   
    }
    //pthread_mutex_unlock(&mutex);
}
