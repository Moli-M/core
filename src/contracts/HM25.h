using namespace QPI;

struct HM252
{
};


struct HM25 : public ContractBase
{
public:
    // --- Funciones básicas (ERC20 y estadísticas) ---

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

    // ERC20: BalanceOf
    struct BalanceOf_input
    {
        id account;
    };
    struct BalanceOf_output
    {
        uint64 balance;
    };

    // ERC20: Transfer
    struct Transfer_input
    {
        id to;
        uint64 amount;
    };
    struct Transfer_output {};

    // --- Funciones DAO ---
    struct CreateProposal_input
    {
        char description[64];
    };
    struct CreateProposal_output {};

    struct VoteProposal_input
    {
        uint64 proposalId;
        bool voteFor;
    };
    struct VoteProposal_output {};

    struct ExecuteProposal_input
    {
        uint64 proposalId;
    };
    struct ExecuteProposal_output {};

private:

    // Datos del token (nombre, símbolo y totalSupply)
    struct Token {
        char name[20];
        char symbol[10];
        uint64 totalSupply;
    };
    Token token;

    // Simulamos un mapping (address => balance) usando un array
    static const uint64 MAX_HOLDERS = 1024;
    struct BalanceEntry {
        id holder;
        uint64 balance;
    };
    Array<BalanceEntry, MAX_HOLDERS> balances;
    uint64 numHolders;

    // Estructuras para la DAO
    struct VoteRecord {
        id voter;
        uint64 weight;
        bool voteFor;
    };

    static const uint64 MAX_PROPOSALS = 128;
    static const uint64 MAX_VOTES_PER_PROPOSAL = MAX_HOLDERS;
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
        if (state.numProposals >= MAX_PROPOSALS) {
            qpi.transfer(qpi.invocator(), qpi.invocationReward());
            return;
        }
        Proposal newProp;
        newProp.proposalId = state.numProposals;
        for (int i = 0; i < 64; i++) {
            newProp.description[i] = input.description[i];
        }
        newProp.votesFor = 0;
        newProp.votesAgainst = 0;
        newProp.executed = false;
        newProp.numVotes = 0;
        state.proposals.set(state.numProposals, newProp);
        state.numProposals++;
    _

    // Vota sobre una propuesta
    PUBLIC_PROCEDURE(VoteProposal)
        if (input.proposalId >= state.numProposals) {
            qpi.transfer(qpi.invocator(), qpi.invocationReward());
            return;
        }
        Proposal prop = state.proposals.get(input.proposalId);
        if (prop.executed) {
            qpi.transfer(qpi.invocator(), qpi.invocationReward());
            return;
        }
        for (int i = 0; i < (int)prop.numVotes; i++) {
            if (prop.voteRecords.get(i).voter == qpi.invocator()) {
                qpi.transfer(qpi.invocator(), qpi.invocationReward());
                return;
            }
        }
        
        // Buscar balance del usuario (antes llamada a FindBalanceIndex)
        const id& account = qpi.invocator();
        int idx = -1;
        for (int i = 0; i < (int)state.numHolders; i++) {
            if (state.balances.get(i).holder == account) {
                idx = i;
                break;
            }
        }
        
        uint64 weight = (idx >= 0) ? state.balances.get(idx).balance : 0;
        if (weight == 0) {
            qpi.transfer(qpi.invocator(), qpi.invocationReward());
            return;
        }
        VoteRecord vote;
        vote.voter = qpi.invocator();
        vote.weight = weight;
        vote.voteFor = input.voteFor;
        if (prop.numVotes < MAX_VOTES_PER_PROPOSAL) {
            prop.voteRecords.set(prop.numVotes, vote);
            prop.numVotes++;
        }
        if (input.voteFor) {
            prop.votesFor += weight;
        } else {
            prop.votesAgainst += weight;
        }
        state.proposals.set(input.proposalId, prop);
    _

    // Ejecuta una propuesta
    PUBLIC_PROCEDURE(ExecuteProposal)
        if (input.proposalId >= state.numProposals) {
            qpi.transfer(qpi.invocator(), qpi.invocationReward());
            return;
        }
        Proposal prop = state.proposals.get(input.proposalId);
        if (prop.executed) {
            qpi.transfer(qpi.invocator(), qpi.invocationReward());
            return;
        }
        if (prop.votesFor > prop.votesAgainst) {
            prop.executed = true;
        }
        state.proposals.set(input.proposalId, prop);
    _

    // --- Funciones ERC20 y estadísticas ---

    // Establece el token y asigna el totalSupply al creador
    PUBLIC_PROCEDURE(SetToken)
        // Copiar datos del token
        for (int i = 0; i < 20; i++) {
            state.token.name[i] = input.name[i];
        }
        for (int i = 0; i < 10; i++) {
            state.token.symbol[i] = input.symbol[i];
        }
        state.token.totalSupply = input.totalSupply;
        
        // Buscar si el creador ya tiene un balance (antes FindBalanceIndex)
        const id& account = qpi.invocator();
        int index = -1;
        for (int i = 0; i < (int)state.numHolders; i++) {
            if (state.balances.get(i).holder == account) {
                index = i;
                break;
            }
        }
        
        if (index >= 0) {
            // Si ya existe, actualizar el balance
            BalanceEntry entry = state.balances.get(index);
            entry.balance = input.totalSupply;
            state.balances.set(index, entry);
        } else {
            // Si no existe, crear nueva entrada (antes AddBalanceEntry)
            if (state.numHolders < MAX_HOLDERS) {
                BalanceEntry entry;
                entry.holder = account;
                entry.balance = input.totalSupply;
                state.balances.set(state.numHolders, entry);
                state.numHolders++;
            }
            // Si se excede MAX_HOLDERS, se debería gestionar el error.
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


    PUBLIC_FUNCTION(BalanceOf)
        // Buscar balance del usuario (antes FindBalanceIndex)
        const id& account = input.account;
        int index = -1;
        for (int i = 0; i < (int)state.numHolders; i++) {
            if (state.balances.get(i).holder == account) {
                index = i;
                break;
            }
        }
        
        if (index >= 0) {
            BalanceEntry entry = state.balances.get(index);
            output.balance = entry.balance;
        } else {
            output.balance = 0;
        }
    _

    // Realiza una transferencia de tokens
    PUBLIC_PROCEDURE(Transfer)
        id sender = qpi.invocator();
        
        // Buscar balance del remitente (antes FindBalanceIndex)
        int senderIndex = -1;
        for (int i = 0; i < (int)state.numHolders; i++) {
            if (state.balances.get(i).holder == sender) {
                senderIndex = i;
                break;
            }
        }
        
        if (senderIndex < 0) {
            qpi.transfer(sender, qpi.invocationReward());
            return;
        }
        
        BalanceEntry senderEntry = state.balances.get(senderIndex);
        if (senderEntry.balance < input.amount) {
            qpi.transfer(sender, qpi.invocationReward());
            return;
        }
        
        // Restar del remitente
        senderEntry.balance -= input.amount;
        state.balances.set(senderIndex, senderEntry);

        // Buscar balance del destinatario (antes FindBalanceIndex)
        int recipientIndex = -1;
        for (int i = 0; i < (int)state.numHolders; i++) {
            if (state.balances.get(i).holder == input.to) {
                recipientIndex = i;
                break;
            }
        }
        
        if (recipientIndex >= 0) {
            // Si ya existe, sumar al balance
            BalanceEntry recipientEntry = state.balances.get(recipientIndex);
            recipientEntry.balance += input.amount;
            state.balances.set(recipientIndex, recipientEntry);
        } else {
            // Si no existe, crear nueva entrada (antes AddBalanceEntry)
            if (state.numHolders < MAX_HOLDERS) {
                BalanceEntry entry;
                entry.holder = input.to;
                entry.balance = input.amount;
                state.balances.set(state.numHolders, entry);
                state.numHolders++;
            }
            // Si se excede MAX_HOLDERS, se debería gestionar el error.
        }
    _

    REGISTER_USER_FUNCTIONS_AND_PROCEDURES
        REGISTER_USER_FUNCTION(GetToken, 1);
        REGISTER_USER_FUNCTION(BalanceOf, 2);
        
        REGISTER_USER_PROCEDURE(CreateProposal, 1);
        REGISTER_USER_PROCEDURE(VoteProposal, 2);
        REGISTER_USER_PROCEDURE(ExecuteProposal, 3);
        REGISTER_USER_PROCEDURE(SetToken, 4);
        REGISTER_USER_PROCEDURE(Transfer, 5);
    _

    INITIALIZE
        state.token.totalSupply = 25;
        for (int i = 0; i < 20; i++) {
            state.token.name[i] = 0;
        }
        for (int i = 0; i < 10; i++) {
            state.token.symbol[i] = 0;
        }
        state.numHolders = 0;
        state.numProposals = 0;
    _
};