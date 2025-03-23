using namespace QPI;

struct HM252
{
};

struct HM25 : public ContractBase
{
public:
    // --- Funciones básicas del contrato (ERC20 y estadísticas) ---
    struct Echo_input {};
    struct Echo_output {};

    struct Burn_input {};
    struct Burn_output {};

    struct SetToken_input
    {
        char name[20];
        char symbol[10];
        uint64 totalSupply;
    };
    struct SetToken_output {};

    struct GetToken_input {};
    struct GetToken_output
    {
        char name[20];
        char symbol[10];
        uint64 totalSupply;
    };

    struct GetStats_input {};
    struct GetStats_output
    {
        uint64 numberOfEchoCalls;
        uint64 numberOfBurnCalls;
    };

    // ERC20: Consulta de saldo
    struct BalanceOf_input
    {
        id account;
    };
    struct BalanceOf_output
    {
        uint64 balance;
    };

    // ERC20: Transferencia
    struct Transfer_input
    {
        id to;
        uint64 amount;
    };
    struct Transfer_output {};

    // --- Funciones de DAO ---
    // Crear una propuesta (por ejemplo, para cambios o decisiones de la DAO)
    struct CreateProposal_input
    {
        char description[64];
    };
    struct CreateProposal_output {};

    // Votar sobre una propuesta
    struct VoteProposal_input
    {
        uint64 proposalId;
        bool voteFor; // true = a favor, false = en contra
    };
    struct VoteProposal_output {};

    // Ejecutar la propuesta (simplemente la marca como ejecutada si "a favor" > "en contra")
    struct ExecuteProposal_input
    {
        uint64 proposalId;
    };
    struct ExecuteProposal_output {};

private:
    // Estadísticas del contrato
    uint64 numberOfEchoCalls;
    uint64 numberOfBurnCalls;

    // Datos del token (nombre, símbolo y totalSupply)
    struct Token {
        char name[20];
        char symbol[10];
        uint64 totalSupply;
    };
    Token token;

    // Mapeo simulado para balances (address => balance) usando un array
    static const uint64 MAX_HOLDERS = 1024;
    struct BalanceEntry {
        id holder;
        uint64 balance;
    };
    Array<BalanceEntry, MAX_HOLDERS> balances;
    uint64 numHolders;

    // Función auxiliar: busca el índice de una cuenta en el array de balances
    int findBalanceIndex(const id &account)
    {
        for (int i = 0; i < (int)numHolders; i++) {
            if (balances.get(i).holder == account) {
                return i;
            }
        }
        return -1;
    }

    // Función auxiliar: agrega una nueva entrada para un holder
    void addBalanceEntry(const id &account, uint64 amount)
    {
        if (numHolders < MAX_HOLDERS) {
            BalanceEntry entry;
            entry.holder = account;
            entry.balance = amount;
            balances.set(numHolders, entry);
            numHolders++;
        }
        // Si se excede MAX_HOLDERS, se debería gestionar el error
    }

    // --- DAO: estructuras internas ---
    struct VoteRecord {
        id voter;
        uint64 weight; // se toma el balance del votante en el momento de votar
        bool voteFor;
    };

    static const uint64 MAX_PROPOSALS = 128;
    static const uint64 MAX_VOTES_PER_PROPOSAL = MAX_HOLDERS; // en el peor caso

    struct Proposal {
        uint64 proposalId;
        char description[64];
        uint64 votesFor;
        uint64 votesAgainst;
        bool executed;
        Array<VoteRecord, MAX_VOTES_PER_PROPOSAL> voteRecords;
        uint64 numVotes;
    };

    Array<Proposal, MAX_PROPOSALS> proposals;
    uint64 numProposals;

    // --- Funciones DAO internas ---
    // Crea una nueva propuesta
    PUBLIC_PROCEDURE(CreateProposal)
        if (numProposals >= MAX_PROPOSALS) {
            // En caso de exceder el número máximo, se retorna sin crear
            qpi.transfer(qpi.invocator(), qpi.invocationReward());
            return;
        }
        Proposal newProp;
        newProp.proposalId = numProposals;
        // Copiar la descripción
        for (int i = 0; i < 64; i++) {
            newProp.description[i] = input.description[i];
        }
        newProp.votesFor = 0;
        newProp.votesAgainst = 0;
        newProp.executed = false;
        newProp.numVotes = 0;
        proposals.set(numProposals, newProp);
        numProposals++;
    _

    // Permite a un holder votar sobre una propuesta; se usa su balance actual como peso
    PUBLIC_PROCEDURE(VoteProposal)
        if (input.proposalId >= numProposals) {
            qpi.transfer(qpi.invocator(), qpi.invocationReward());
            return;
        }
        Proposal prop = proposals.get(input.proposalId);
        if (prop.executed) {
            // No se pueden votar propuestas ya ejecutadas
            qpi.transfer(qpi.invocator(), qpi.invocationReward());
            return;
        }
        // Verificar si el votante ya ha votado en esta propuesta
        for (int i = 0; i < (int)prop.numVotes; i++) {
            if (prop.voteRecords.get(i).voter == qpi.invocator()) {
                // Ya votó, no permitir votar de nuevo
                qpi.transfer(qpi.invocator(), qpi.invocationReward());
                return;
            }
        }
        // Obtener el peso (balance) del votante
        int idx = findBalanceIndex(qpi.invocator());
        uint64 weight = (idx >= 0) ? balances.get(idx).balance : 0;
        // Si no tiene tokens, no puede votar
        if (weight == 0) {
            qpi.transfer(qpi.invocator(), qpi.invocationReward());
            return;
        }
        // Crear el registro de voto
        VoteRecord vote;
        vote.voter = qpi.invocator();
        vote.weight = weight;
        vote.voteFor = input.voteFor;
        // Agregarlo a la propuesta
        if (prop.numVotes < MAX_VOTES_PER_PROPOSAL) {
            prop.voteRecords.set(prop.numVotes, vote);
            prop.numVotes++;
        }
        // Acumular votos según el voto
        if (input.voteFor) {
            prop.votesFor += weight;
        } else {
            prop.votesAgainst += weight;
        }
        proposals.set(input.proposalId, prop);
    _

    // Ejecuta la propuesta: si votos a favor superan a los en contra, se marca como ejecutada
    PUBLIC_PROCEDURE(ExecuteProposal)
        if (input.proposalId >= numProposals) {
            qpi.transfer(qpi.invocator(), qpi.invocationReward());
            return;
        }
        Proposal prop = proposals.get(input.proposalId);
        if (prop.executed) {
            // Ya fue ejecutada
            qpi.transfer(qpi.invocator(), qpi.invocationReward());
            return;
        }
        // Se decide la propuesta si los votos a favor superan a los en contra
        if (prop.votesFor > prop.votesAgainst) {
            // Se podría incluir lógica para ejecutar alguna acción de la DAO.
            // Por ahora, simplemente marcamos la propuesta como ejecutada.
            prop.executed = true;
        }
        proposals.set(input.proposalId, prop);
    _

    // --- Funciones ERC20 y estadísticas ---
    PUBLIC_PROCEDURE(Echo)
        state.numberOfEchoCalls++;
        if (qpi.invocationReward() > 0)
        {
            qpi.transfer(qpi.invocator(), qpi.invocationReward());
        }
    _

    PUBLIC_PROCEDURE(Burn)
        state.numberOfBurnCalls++;
        if (qpi.invocationReward() > 0)
        {
            qpi.burn(qpi.invocationReward());
        }
    _

    // Establece el token y asigna el totalSupply al creador
    PUBLIC_PROCEDURE(SetToken)
        Token tempToken;
        for (int i = 0; i < 20; i++) {
            tempToken.name[i] = input.name[i];
        }
        for (int i = 0; i < 10; i++) {
            tempToken.symbol[i] = input.symbol[i];
        }
        tempToken.totalSupply = input.totalSupply;
        state.token = tempToken;

        // Asigna el totalSupply al creador en el "mapping" de balances
        int index = findBalanceIndex(qpi.invocator());
        if (index >= 0) {
            BalanceEntry entry = balances.get(index);
            entry.balance = input.totalSupply;
            balances.set(index, entry);
        } else {
            addBalanceEntry(qpi.invocator(), input.totalSupply);
        }
    _

    PUBLIC_FUNCTION(GetToken)
        for (int i = 0; i < 20; i++) {
            output.name[i] = state.token.name[i];
        }
        for (int i = 0; i < 10; i++) {
            output.symbol[i] = state.token.symbol[i];
        }
        output.totalSupply = state.token.totalSupply;
    _

    PUBLIC_FUNCTION(GetStats)
        output.numberOfEchoCalls = state.numberOfEchoCalls;
        output.numberOfBurnCalls = state.numberOfBurnCalls;
    _

    // Consulta el balance de una cuenta
    PUBLIC_FUNCTION(BalanceOf)
        int index = findBalanceIndex(input.account);
        if (index >= 0) {
            BalanceEntry entry = balances.get(index);
            output.balance = entry.balance;
        } else {
            output.balance = 0;
        }
    _

    // Realiza una transferencia de tokens
    PUBLIC_PROCEDURE(Transfer)
        id sender = qpi.invocator();
        int senderIndex = findBalanceIndex(sender);
        if (senderIndex < 0) {
            qpi.transfer(sender, qpi.invocationReward());
            return;
        }
        BalanceEntry senderEntry = balances.get(senderIndex);
        if (senderEntry.balance < input.amount) {
            qpi.transfer(sender, qpi.invocationReward());
            return;
        }
        senderEntry.balance -= input.amount;
        balances.set(senderIndex, senderEntry);

        int recipientIndex = findBalanceIndex(input.to);
        if (recipientIndex >= 0) {
            BalanceEntry recipientEntry = balances.get(recipientIndex);
            recipientEntry.balance += input.amount;
            balances.set(recipientIndex, recipientEntry);
        } else {
            addBalanceEntry(input.to, input.amount);
        }
    _

    REGISTER_USER_FUNCTIONS_AND_PROCEDURES
        // Funciones/procedimientos ERC20 y estadísticas
        REGISTER_USER_PROCEDURE(Echo, 1);
        REGISTER_USER_PROCEDURE(Burn, 2);
        REGISTER_USER_PROCEDURE(SetToken, 3);
        REGISTER_USER_PROCEDURE(Transfer, 4);

        REGISTER_USER_FUNCTION(GetStats, 1);
        REGISTER_USER_FUNCTION(GetToken, 2);
        REGISTER_USER_FUNCTION(BalanceOf, 3);

        // Funciones/procedimientos DAO
        REGISTER_USER_PROCEDURE(CreateProposal, 5);
        REGISTER_USER_PROCEDURE(VoteProposal, 6);
        REGISTER_USER_PROCEDURE(ExecuteProposal, 7);
    _

    INITIALIZE
        state.numberOfEchoCalls = 0;
        state.numberOfBurnCalls = 0;
        // Inicialización por defecto del token: totalSupply en 25, nombre y símbolo vacíos.
        state.token.totalSupply = 25;
        for (int i = 0; i < 20; i++) {
            state.token.name[i] = 0;
        }
        for (int i = 0; i < 10; i++) {
            state.token.symbol[i] = 0;
        }
        numHolders = 0;
        numProposals = 0;
    _
};
