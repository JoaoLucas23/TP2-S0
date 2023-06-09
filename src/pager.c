#include "pager.h"
#include "mmu.h"

#include <sys/mman.h>
#include <assert.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

// Estrutura dos quadros da memória.
typedef struct {
	pid_t pid;
    int numero;
	int pagina;
	short usando; // 0 se não está em uso, 1 caso contrário.
	short referencia;
	short none;
	short escrito;
} quadro_t;

typedef struct {
    int numero;
	short usando; // 0 se não está em uso, 1 caso contrário.
} bloco_t;

// Estrutura das tabelas de páginas de cada processo.
typedef struct {
	int tamanho; // Usado como tamanho dos dois vetores frames e blocks.
	quadro_t *quadros; // Vetor de quadros do processo.
	bloco_t *blocos; // Vetor de blocos do processo.
} pagina_t;

// Estrutura da lista de tabela de páginas.
typedef struct
{
	pid_t pid;
    int tamanho;
	pagina_t *paginas;
} tabela_t;

int qtd_blocos;
int qtd_quadros;
int blocos_livres;
int qtd_tabelas;

int clock_ptr;

bloco_t *blocos;
quadro_t *quadros;
tabela_t *tabelas;

void pager_init(int nframes, int nblocks) {
    qtd_tabelas = 1;
    tabelas = (tabela_t*) malloc (sizeof(tabela_t));

    blocos = (bloco_t*) malloc (nblocks * sizeof(bloco_t));
    qtd_blocos = nblocks;
    blocos_livres = nblocks;
    for(int i=0; i< nblocks; i++) {
        blocos[i].usando = 0;
        blocos[i].numero = 0;
    }

    clock_ptr = 0;

    qtd_quadros = nframes;
    quadros = (quadro_t*) malloc (nframes * sizeof(quadro_t));

    for(int i = 0; i < nframes; i++) {
		quadros[i].pid = -1;
		quadros[i].pagina = 0;
        quadros[i].numero = 0;
		quadros[i].usando = 0;
		quadros[i].referencia = 0;
		quadros[i].none = 1;
		quadros[i].escrito = 0;
	}
}

void pager_create(pid_t pid)
{
	// Calcula o número de páginas dos vetores frames e blocks.
	int num_pages = (UVM_MAXADDR - UVM_BASEADDR + 1) / sysconf(_SC_PAGESIZE);

    int flag = 0;

	// Procurando tabela vazia na lista de tabelas.
	for(int i = 0; i < qtd_tabelas; i++)
	{
		// Se a tabela estiver vazia.
		if(tabelas[i].paginas == NULL)
		{
			// Atribui o pid do processo a tabela.
			tabelas[i].pid = pid;
			// Inicializa a tabela de página.
			tabelas[i].paginas = (pagina_t*) malloc (sizeof(pagina_t));
			tabelas[i].paginas->tamanho = num_pages;
			// Inicializa os vetores frames e blocks.
			tabelas[i].paginas->quadros = (quadro_t*) malloc (num_pages * sizeof(quadro_t));
			tabelas[i].paginas->blocos = (bloco_t*) malloc (num_pages * sizeof(bloco_t));

			// Seta os valores para -1 (convenção de vazio).
			for(int j = 0; j < num_pages; j++)
			{
				tabelas[i].paginas->quadros[j].numero = -1;
                tabelas[i].paginas->quadros[j].usando = 0;
				tabelas[i].paginas->blocos[j].numero = -1;
                tabelas[i].paginas->blocos[j].usando = 0;
			}
			flag = 1; // Flag usada para indicar se achou uma tabela vazia.
			break;
		}
	}

	// Se não achou uma tabela vazia.
	if(flag == 0)
	{
		// Aumenta o tamanho da lista de tabelas.
		tabelas = realloc(tabelas, (100 + qtd_tabelas) * sizeof(tabela_t));

		// Aloca o novo processo a primeira posição vazia.
		tabelas[qtd_tabelas].pid = pid;
		tabelas[qtd_tabelas].paginas = (pagina_t*) malloc (sizeof(pagina_t));
		tabelas[qtd_tabelas].paginas->tamanho = num_pages;
		tabelas[qtd_tabelas].paginas->quadros = (quadro_t*) malloc (num_pages * sizeof(quadro_t));
		tabelas[qtd_tabelas].paginas->blocos = (bloco_t*) malloc (num_pages * sizeof(bloco_t));
		for(int j = 0; j < num_pages; j++)
		{
            tabelas[qtd_tabelas].paginas->quadros[j].usando = 0;
			tabelas[qtd_tabelas].paginas->quadros[j].numero = -1;
			tabelas[qtd_tabelas].paginas->blocos[j].numero = -1;
            tabelas[qtd_tabelas].paginas->blocos[j].usando = 0;
		}
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

	// Pegando o primeiro bloco vazio.
	for(int i = 0; i < qtd_blocos; i++)
	{
		if(blocos[i].usando == 0) //Se o bloco na posição i não é usado.
		{
			blocos[i].usando = 1; // Seta o bloco para usado.
            blocos[i].numero = i;
		    blocos_livres--; // Decrementa o contador de blocos livres.
			bloco = blocos[i]; // Variável auxiliar para salvar a posição do bloco.
			break;
		}
	}

	// Localizando a tabela de página do processo pid.
	for(int i = 0; i < qtd_tabelas; i++)
	{
		if(tabelas[i].pid == pid)
		{
			// Procuro um bloco vazio na tabela do processo.
			for(int j = 0; j < tabelas[i].paginas->tamanho; j++)
			{
				if(tabelas[i].paginas->blocos[j].numero == -1)
				{
					// Salvo o indice do vetor de blocos, no bloco do processo.
					tabelas[i].paginas->blocos[j] = bloco;
					pos = j;
					break;
				}
				// Se não há blocos livres na tabela do processo.
				if(j == (tabelas[i].paginas->tamanho) - 1){
					return NULL;
				}
			}
			break;
		}
	}

	// Retorna o endereço (Inicio + posição * tamanho da pagina).
	return (void*) (UVM_BASEADDR + (intptr_t) (pos * sysconf(_SC_PAGESIZE)));
}

void pager_fault(pid_t pid, void *vaddr) {
int i, index, index2, page_num, curr_frame, new_frame, curr_block, new_block,
		move_disk_pid, move_disk_pnum, mem_no_none;
	void *addr;

	// Procura o índice da tabela de página do processo pid na lista de tabelas.
	for(i = 0; i < qtd_tabelas; i++)
	{
		if(tabelas[i].pid == pid)
		{
			// Salva o indice e sai.
			index = i;
			break;
		}
	}

	// Pega o número do quadro na tabela do processo.
	page_num = ((((intptr_t) vaddr) - UVM_BASEADDR) / (sysconf(_SC_PAGESIZE)));

	mem_no_none = 1;
	for(i = 0; i < qtd_quadros; i++)
	{
		if(quadros[i].none == 1)
		{
			mem_no_none = 0;
			break;
		}
	}
	// Se esse quadro está carregado.
	if(tabelas[index].paginas->quadros[page_num].numero != -1 && tabelas[index].paginas->quadros[page_num].numero != -2)
	{
		// Salva o índice do vetor de quadros (memória).
		curr_frame = tabelas[index].paginas->quadros[page_num].numero;
		// Dá permissão de escrita para o processo pid.
		mmu_chprot(pid, vaddr, PROT_READ | PROT_WRITE);
		// Marca o bit de referência no vetor de quadros (memória).
		quadros[curr_frame].referencia = 1;
		//marca que houve escrita
		quadros[curr_frame].escrito = 1;
	}
	else // Se não está carregado:
	{
		if(mem_no_none)
		{
			for(i = 0; i < qtd_quadros; i++)
			{
				addr = (void*) (UVM_BASEADDR + (intptr_t) (quadros[i].pagina * sysconf(_SC_PAGESIZE)));
				mmu_chprot(quadros[i].pid, addr, PROT_NONE);
				quadros[i].none = 1;
			}
		}
		new_frame = -1;
		while(new_frame == -1)
		{
			new_frame = -1;
			// Se o bit de referência é zero.
			if(quadros[clock_ptr].referencia == 0)
			{
				new_frame = clock_ptr;
				// Se o frame está em uso.
				if(quadros[clock_ptr].usando == 1)
				{
					// Remove o frame e Salva o frame no disco se tiver permissão de escrita.
					move_disk_pid = quadros[clock_ptr].pid;
					move_disk_pnum = quadros[clock_ptr].pagina;
					for(i = 0; i < qtd_tabelas; i++)
					{
						if(tabelas[i].pid == move_disk_pid)
						{
							index2 = i;
						}
					}

					curr_block = tabelas[index2].paginas->blocos[move_disk_pnum].numero;
					mmu_nonresident(pid, (void*) (UVM_BASEADDR + (intptr_t) (move_disk_pnum * sysconf(_SC_PAGESIZE))));
					if(quadros[clock_ptr].escrito == 1)
					{
						mmu_disk_write(clock_ptr, curr_block);
						// Marca o frame como vazio (sem uso) que está no disco
						tabelas[index2].paginas->quadros[move_disk_pnum].numero = -2;
					}
					else
					{
						// Marca o frame como vazio (sem uso).
						tabelas[index2].paginas->quadros[move_disk_pnum].numero = -1;
					}

				}
				// Coloca o novo processo no vetor de quadros.
				quadros[clock_ptr].pid = pid;
				quadros[clock_ptr].pagina = page_num;
				quadros[clock_ptr].usando = 1;
				quadros[clock_ptr].referencia = 1;
				quadros[clock_ptr].none = 0;

				if(tabelas[index].paginas->quadros[page_num].numero == -2)
				{
					new_block = tabelas[index].paginas->blocos[page_num].numero;
					mmu_disk_read(new_block, new_frame);
					quadros[clock_ptr].escrito = 1;
				}
				else
				{
					mmu_zero_fill(new_frame);
					quadros[clock_ptr].escrito = 0;
				}
				tabelas[index].paginas->quadros[page_num].numero = new_frame;
				mmu_resident(pid, vaddr, new_frame, PROT_READ);
			}
			else
			{
				quadros[clock_ptr].referencia = 0;
			}
			clock_ptr++;
			clock_ptr %= qtd_quadros;
		}
	}
}

int pager_syslog(pid_t pid, void *addr, size_t len) {
	int i, j, index, frame_limit, flag;
	char *message = (char*) malloc (len + 1);

	// Pegando o índice da tabela de página do processo pid.
	for(i = 0; i < qtd_tabelas; i++)
	{
		if(tabelas[i].pid == pid)
		{
			index = i;
			break;
		}
	}

	// Pegando o indice do primeiro frame vazio no vetor de frames do processo.
	for(i = 0; i < tabelas[index].paginas->tamanho; i++)
	{
		if(tabelas[index].paginas->quadros[i].numero == -1)
		{
			frame_limit = i;
			break;
		}
	}

	for(i = 0; i < len; i++)
	{
		flag = 1;
		for(j = 0; j < frame_limit; j++)
		{
			// Se está acessando um frame permitido ao processo pid (que tem a tabela tabelas[index].paginas)
			if(((intptr_t) addr + i - UVM_BASEADDR) / (sysconf(_SC_PAGESIZE)) == tabelas[index].paginas->quadros[j].numero)
			{
				flag = 0;
				break;
			}
		}

		if(flag)
			return -1; // Caso sem permissão.

		// Soma o índice i, arredonda (fazendo o AND, para retirar os 1's menos significativos),
		// subtrai a primeira posição e divide pelo tamanho do frame.
		// Isso é usado para conseguir o índice do frame que deve ser lido.
		int pag = ((((intptr_t) addr + i)) - UVM_BASEADDR) / (sysconf(_SC_PAGESIZE));
		// Pega o índice do dado no frame_vector.
		int frame_index = tabelas[index].paginas->quadros[pag].numero;
		message[i] = pmem[(frame_index * sysconf(_SC_PAGESIZE)) + i];
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