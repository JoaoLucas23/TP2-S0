<!-- LTeX: language=pt-BR -->

# PAGINADOR DE MEMÓRIA -- RELATÓRIO

1. Termo de compromisso

    Ao entregar este documento preenchiso, os membros do grupo afirmam que todo o código desenvolvido para este trabalho é de autoria própria.  Exceto pelo material listado no item 3 deste relatório, os membros do grupo afirmam não ter copiado material da Internet nem ter obtido código de terceiros.

2. Membros do grupo e alocação de esforço

    Preencha as linhas abaixo com o nome e o email dos integrantes do grupo.  Substitua marcadores `XX` pela contribuição de cada membro do grupo no desenvolvimento do trabalho (os valores devem somar 100%).

    * João Lucas Lage Gonçalves <jllgoncalves23@gmail.com> 50%
    * João Vitor Mateus Silva <joaovitor2207@gmail.com> 50%

3. Referências bibliográficas
    https://man7.org/linux/man-pages/man3/sysconf.3.html
    https://www.geeksforgeeks.org/second-chance-or-clock-page-replacement-policy/

4. Detalhes de implementação

    1. Descreva e justifique as estruturas de dados utilizadas em sua solução.
    quadro_t: Essa estrutura representa um quadro de memória física. Ela contém informações como o ID do processo que ocupa o quadro, o número do quadro, o número da página, uma flag indicando se está em uso, uma flag indicando se foi referenciado, uma flag indicando se está vazio (none) e uma flag indicando se foi escrito.

        bloco_t: Essa estrutura representa um bloco de memória virtual. Ela contém informações como o número do bloco e uma flag indicando se está em uso.

        pagina_t: Essa estrutura representa a tabela de páginas de um processo. Ela contém o tamanho da tabela, um vetor de quadros representando os quadros da memória física ocupados pelo processo e um vetor de blocos representando os blocos de memória virtual ocupados pelo processo.

        tabela_t: Essa estrutura representa a lista de tabelas de páginas. Ela contém o ID do processo, o tamanho da tabela de páginas desse processo e um ponteiro para a tabela de páginas.

        A estrutura quadro_t é utilizada para armazenar informações sobre cada quadro de memória física, como o processo que o ocupa e o número da página que está mapeada nele. Essas informações são essenciais para o algoritmo de substituição de página e para a manutenção do estado dos quadros.

        A estrutura bloco_t é utilizada para armazenar informações sobre cada bloco de memória virtual ocupado por um processo. Ela permite verificar se um bloco está em uso ou livre, o que é necessário para a alocação de novos blocos no processo.

        A estrutura pagina_t representa a tabela de páginas de um processo. Ela contém vetores de quadros e blocos, que são utilizados para armazenar informações sobre os quadros e blocos ocupados pelo processo. Essa estrutura é importante para manter o mapeamento entre os quadros de memória física e os blocos de memória virtual do processo.

        A estrutura tabela_t representa a lista de tabelas de páginas. Ela é utilizada para armazenar as tabelas de páginas de todos os processos em execução. Cada entrada na lista contém o ID do processo e um ponteiro para a tabela de páginas desse processo. Essa estrutura permite localizar rapidamente a tabela de páginas de um processo pelo seu ID.

        Essas estruturas de dados foram escolhidas porque fornecem uma representação eficiente e organizada das informações necessárias para o gerenciamento da memória virtual. Elas permitem o acesso rápido aos quadros e blocos ocupados pelos processos, além de facilitar a implementação dos algoritmos de substituição de página e das operações de alocação e desalocação de memória

    2. Descreva o mecanismo utilizado para controle de acesso e modificação às páginas.
        No código anterior, o mecanismo utilizado para controle de acesso e modificação às páginas é baseado nas flags presentes na estrutura quadro_t. Essas flags indicam se o quadro está em uso, se foi referenciado e se foi escrito.

        Quando um processo acessa uma página, o mecanismo de controle verifica se o quadro correspondente à página está em uso. Caso não esteja, é feita uma operação de falta de página (page fault), indicando que a página precisa ser buscada na memória secundária e carregada na memória física.

        Durante a busca da página na memória secundária, a flag "referenciado" é utilizada para marcar a página como referenciada. Isso é útil para os algoritmos de substituição de página, pois eles podem levar em consideração essa informação ao decidir qual página substituir.

        Quando o processo realiza uma operação de escrita na página, a flag "escrito" é marcada para indicar que a página foi modificada. Isso é importante para garantir a consistência dos dados e para possibilitar a escrita de volta na memória secundária quando necessário.

        Essas flags são atualizadas e manipuladas durante a execução do algoritmo de gerenciamento de memória virtual, garantindo que o acesso e a modificação às páginas sejam controlados e rastreados corretamente. O uso dessas flags permite ao sistema operacional gerenciar eficientemente a memória, realizando operações de substituição de página quando necessário e mantendo a consistência dos dados entre a memória física e a memória secundária.