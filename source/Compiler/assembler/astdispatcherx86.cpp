#include "astdispatcherx86.h"


ASTdispatcherX86::ASTdispatcherX86()
{

}

void ASTdispatcherX86::dispatch(QSharedPointer<NodeBinOP>node)
{
        if (node->m_left->isWord(as) && !node->m_right->isPointer(as))
            node->m_right->setForceType(TokenType::INTEGER);
        if (node->m_right->isWord(as) && !node->m_left->isPointer(as))
            node->m_left->setForceType(TokenType::INTEGER);

/*        if (node->m_left->isWord(as))
            node->m_right->setForceType(TokenType::INTEGER);
        if (node->m_right->isWord(as))
            node->m_left->setForceType(TokenType::INTEGER);
*/
    as->ClearTerm();
    if (!node->isPointer(as)) {
        if (node->isPureNumeric()) {
            as->Asm("mov "+getAx(node)+", " + node->getValue(as) + "; binop is pure numeric");
            return;
        }
        if (node->isPureVariable()) {
            as->Asm("mov "+getAx(node)+", [" + node->getValue(as)+"] ; binop is pure variable");
            return;
        }
    }
/*    if (!node->m_left->isPure() && node->m_right->isPure()) {
        QSharedPointer<Node> t = node->m_right;
        node->m_right = node->m_left;
        node->m_left = t;
        qDebug() << "SWITCH";
    }
*/
    if (node->m_op.m_type == TokenType::MUL || node->m_op.m_type == TokenType::DIV) {
        node->m_left->Accept(this);
        QString bx = getAx(node->m_left);

        PushX();
        QString sign = "";
        bool isSigned = false;
        if (node->m_left->isSigned(as)||node->m_right->isSigned(as)) {
            sign = "i";
            isSigned = true;
        }
        if (node->m_op.m_type == TokenType::DIV) {
            //            node->m_right->setForceType(TokenType::BYTE);
            if (node->m_right->isWord(as)) {
                as->Asm("xor dx,dx");
                if (isSigned)
                    as->Asm("cwd");
            }
            else {
                if (isSigned) as->Asm("cbw");
                as->Asm("xor ah,ah");
            }
        }
        node->m_right->Accept(this);

        QString ax = getAx(node->m_right);
        PopX();
        as->BinOP(node->m_op.m_type);
        if (bx[0]!='a')  {
            as->Asm("push ax");
            as->Asm("mov ax,"+bx);
        }
        as->Asm(sign+as->m_term +" " +  ax);
        if (bx[0]!='a')  {
            as->Asm("mov "+bx+",ax");
            as->Asm("pop ax");
        }
        as->m_term = "";
        return;
    }
    if (node->m_left->isPointer(as) || node->m_right->isPointer(as)) {
        // Treat as pointers
        // Make sure left is pointer
        if (!node->m_left->isPointer(as)) {
            auto n = node->m_right;
            node->m_right = node->m_left;
            node->m_left = n;
        }
        as->ClearTerm();
        as->BinOP(node->m_op.m_type);
        QString bop = as->m_term;
        as->ClearTerm();

        if (node->m_right->isPointer(as)) {
            if (!node->m_right->isPure()) {
//                ErrorHandler::e.Error("Pointers can only add / sub with other pure pointers.", node->m_op.m_lineNumber);
                as->Comment("Interesting..");
                as->ClearTerm();
                node->m_right->Accept(this);
                as->Asm("push ax");

                QString ax =getAx(node->m_right);
                as->Comment("Interesting ends");
                node->m_left->Accept(this);

                QString bx =getAx(node->m_left);
                as->Asm("pop bx");
                as->Asm(bop + " ax,bx");
                return;

            }

            QString r = node->m_right->getValue(as);
            if (!node->m_right->isPointer(as)) {
                node->m_right->setForceType(TokenType::INTEGER);
                as->Comment("RHS is pure integer");
                return;
            }
            as->Comment("RHS is pure pointer as well! "+r);
            node->m_left->Accept(this); // Should always be a pointer
            as->Term();

            as->Asm("mov ax,es");
            as->Asm(bop+" ax,["+r+"+2]");
            as->Asm("mov es,ax");
            as->Asm(bop+" di,["+r+"]");
            return;
        }

        as->Comment("RHS is NOT pointer, only updating DI");

        as->ClearTerm();
//        node->m_right->setForceType(TokenType::INTEGER);
        node->m_right->Accept(this);
        as->Term();
        if (!node->m_left->isPure())
            ErrorHandler::e.Error("Left-hand side of equation must be pure pointer and not a binary operation!", node->m_op.m_lineNumber);

        node->m_left->Accept(this); // Should always be a PURE pointer
        as->Asm(bop+" di,ax");

        return;
    }

    if (node->m_right->isPure() && (node->m_op.m_type==TokenType::PLUS || node->m_op.m_type==TokenType::MINUS || node->m_op.m_type==TokenType::BITAND || node->m_op.m_type==TokenType::BITOR || node->m_op.m_type==TokenType::XOR)) {
        as->Comment("RHS is pure optimization");

        as->ClearTerm();
        node->m_left->Accept(this);
        QString ax = getAx(node);
        as->Term();
        as->Comment("Forcetype IS POINTER: "+QString::number(node->m_forceType==TokenType::POINTER));
        as->BinOP(node->m_op.m_type);
        if (node->m_left->isReference())
            ax="di";
        as->Asm(as->m_term + " "+ax+", "+getX86Value(as,node->m_right));
        as->ClearTerm();
        return;

    }


    as->Comment("Generic add/sub");
    node->m_left->Accept(this);
    QString bx = getAx(node->m_left);
    if (m_isPurePointer)
        bx = "di";


    PushX();
    if (node->m_op.m_type==TokenType::SHR || node->m_op.m_type==TokenType::SHL)
        PushX();
    node->m_right->Accept(this);
//    as->Term();
    QString ax = getAx(node->m_right);
    if (node->m_op.m_type==TokenType::SHR || node->m_op.m_type==TokenType::SHL) {
       PopX();
       ax = "cl";
    }
    PopX();
    as->BinOP(node->m_op.m_type);

    as->Asm(as->m_term + " " +  bx +"," +ax);
    as->m_term = "";


}

void ASTdispatcherX86::dispatch(QSharedPointer<NodeNumber>node)
{
    QString ax = getAx(node);
    if (as->m_term!="") {
        as->m_term +=node->getValue(as);
        return;
    }

    as->Asm("mov "+ax+", " + node->getValue(as));
}

void ASTdispatcherX86::dispatch(QSharedPointer<NodeVar> node)
{
    if (m_inlineParameters.contains(node->value)) {
  //      qDebug()<< "INLINE node override : "<< node->value;
        m_inlineParameters[node->value]->Accept(this);
        return;
    }
    QString ending = "]";
    if (node->m_expr!=nullptr) {

        if (node->getArrayType(as)==TokenType::POINTER && node->m_classApplied==false) {
            as->Comment("Looking up array of pointer : "+node->value);

            node->m_expr->setForceType(TokenType::INTEGER);
            node->m_expr->Accept(this);
            as->Asm("mov bx,ax");
            as->Asm("shl bx,2");

            as->Asm("lea si,["+node->getValue(as)+ "]");
            as->Asm("mov di,[ds:si+bx]");
            as->Asm("mov ax,[ds:si+bx+2]");
            as->Asm("mov es,ax");
            //            as->Asm("mov di,cx");

            return;
        }

        if (node->isPointer(as)) {
            //            as->Asm("push ax");
            as->Asm("les di,["+node->getValue(as)+ "]");
            if (node->m_expr->isPureNumeric()) {
                as->Asm("mov ax, word [es:di + "+node->m_expr->getValue(as)+"*" + getIndexScaleVal(as,node)+"]");
                if (!node->isWord(as))
                    as->Asm("mov ah,0"); // Force byte
                return;

            }
            if (node->m_expr->isPureVariable() && !node->m_expr->isArrayIndex()) {
                as->Asm("add di,word ["+node->m_expr->getValue(as)+"]");
                if (node->isWord(as))
                    as->Asm("add di,word ["+node->m_expr->getValue(as)+"]");
                as->Asm("mov ax, word [es:di]");
                if (!node->isWord(as))
                    as->Asm("mov ah,0");
                return;

            }
            node->m_expr->setForceType(TokenType::INTEGER);
            node->m_expr->Accept(this);
            as->Asm("add di,ax");
            if (node->getArrayType(as)==TokenType::INTEGER)
                //                as->Asm("shl di,1 ; Accomodate for word");
                as->Asm("add di,ax");


            //          as->Asm("pop ax");
            if (node->isWord(as))
                as->Asm("mov ax, word [es:di]; Is word");
            else
                as->Asm("mov al, byte [es:di]; Is byte" );
            return;
        }
        if (node->is8bitValue(as))
            as->Asm("mov ah,0 ; Accomodate for byte");
/*
        node->m_expr->setForceType(TokenType::INTEGER);
        node->m_expr->Accept(this);
        as->Asm("mov di,ax");
        if (node->getArrayType(as)==TokenType::INTEGER)
            as->Asm("shl di,1 ; Accomodate for word");
        ending = "+di]";
        */
        node->m_expr->setForceType(TokenType::INTEGER);

        if (node->m_expr->isPure()) {
            if (node->m_expr->isPureNumeric()) {
                int mul = 1;
                if (node->getArrayType(as)==TokenType::INTEGER)
                    mul = 2;
                if (node->getArrayType(as)==TokenType::LONG)
                    mul = 4;
                as->Asm("mov di,"+Util::numToHex(node->m_expr->getValueAsInt(as)*mul));
            }
            else {

                as->Asm("mov di,"+getX86Value(as,node->m_expr));
                if (node->getArrayType(as)==TokenType::INTEGER)
                    as->Asm("shl di,1 ; Accomodate for word");
            }
        }
        else {
            node->m_expr->Accept(this);
            as->Asm("mov di,ax");
            if (node->getArrayType(as)==TokenType::INTEGER)
                as->Asm("shl di,1 ; Accomodate for word");
        }
        ending = "+di]";

    }

/*    if (node->m_forceType==TokenType::POINTER && !node->isPointer(as)) {
        as->Comment("Force type is POINTER, converting");
        as->Asm("mov es,0");
        as->Asm("mov di, [" + node->getValue(as)+"]");
        return;
    }
*/
    if (node->isReference()) {
        as->Asm("mov di, " + node->getValue(as)+"");
        as->Asm("push ds");
        as->Asm("pop es");
        return;
    }

    if (node->isPointer(as) && !node->isArrayIndex()) {
        as->Asm("les di, [" + node->getValue(as)+"]");
        return;
    }

    if (as->m_term!="") {
        as->m_term +=node->getValue(as);
        return;
    }
    QString ax = getAx(node);




    as->Asm("mov "+ax+", [" + node->getValue(as)+ending);
//    if (node->isArrayIndex())
//        qDebug() << TokenType::getType(node->getArrayType(as));
    if (node->m_forceType==TokenType::INTEGER) {
        bool accomodate = false;
        if (node->isArrayIndex()) {
            if (node->getArrayType(as)!=TokenType::INTEGER) {
            accomodate = true;
            }
        }
        else
        if (node->getOrgType(as)!=TokenType::INTEGER)
            accomodate = true;

        if (accomodate && !node->isPointer(as)) {
            QString aa = QString(getAx(node)[0]);
            as->Asm("mov "+QString(ax[0])+"h,0 ; forcetype clear high bit");
        }
    }
//    qDebug() << "ORG " <<TokenType::getType(node->getOrgType(as)) << "   : " << node->getValue(as);
  //  qDebug() << "FT " <<TokenType::getType(node->m_forceType);

}


void ASTdispatcherX86::dispatch(QSharedPointer<NodeString> node)
{

}



void ASTdispatcherX86::dispatch(QSharedPointer<NodeVarType> node)
{

}

void ASTdispatcherX86::dispatch(QSharedPointer<NodeBinaryClause> node)
{

}


void ASTdispatcherX86::dispatch(QSharedPointer<Node> node)
{

}

void ASTdispatcherX86::dispatch(QSharedPointer<NodeAssign> node)
{
/*    if (node==nullptr)
        return;*/
//    node->DispatchConstructor(as,this);
    node->m_currentLineNumber = node->m_op.m_lineNumber;


    AssignVariable(node);

}

void ASTdispatcherX86::dispatch(QSharedPointer<NodeRepeatUntil> node)
{
    ErrorHandler::e.Error("Repeat-until not implemented yet", node->m_op.m_lineNumber);

}
void ASTdispatcherX86::dispatch(QSharedPointer<NodeComment> node)
{

}

void ASTdispatcherX86::StoreVariable(QSharedPointer<NodeVar> n)
{

}

void ASTdispatcherX86::LoadVariable(QSharedPointer<NodeVar> n)
{
    n->Accept(this);
}

void ASTdispatcherX86::LoadAddress(QSharedPointer<Node> n)
{

}

void ASTdispatcherX86::LoadAddress(QSharedPointer<Node> n, QString reg)
{

}

void ASTdispatcherX86::LoadVariable(QSharedPointer<NodeProcedure> node)
{
    ErrorHandler::e.Error("Procedure address not implemented yet! Please bug the developer", node->m_op.m_lineNumber);
}

void ASTdispatcherX86::LoadPointer(QSharedPointer<Node> n)
{

}

void ASTdispatcherX86::LoadVariable(QSharedPointer<Node> n)
{
    n->Accept(this);

}

void ASTdispatcherX86::LoadVariable(QSharedPointer<NodeNumber>n)
{

}

QString ASTdispatcherX86::getIndexScaleVal(Assembler *as, QSharedPointer<Node> var)
{
    if (var->isWord(as))
        return "2";
    if (var->isLong(as))
        return "4";
    return "1";
}

QString ASTdispatcherX86::getAx(QSharedPointer<Node> n) {
    QString a = m_regs[m_lvl];


    if (n->m_forceType==TokenType::INTEGER)
        return a+"x";
    if (n->getType(as)==TokenType::INTEGER)
        return a+"x";
    if (n->getType(as)==TokenType::ADDRESS)
        return a+"x";
    if (n->getType(as)==TokenType::INTEGER_CONST)
        if (n->isWord(as))
            return a+"x";
    //        if (n->isPureNumeric())
    //          if (n->getValue()
    return a+"l";
}

QString ASTdispatcherX86::getAx(QString a, QSharedPointer<Node> n) {


    if (n->m_forceType==TokenType::INTEGER)
        return a+"x";
    if (n->isWord(as))
        return a+"x";

    return a+"l";


}

QString ASTdispatcherX86::getBinaryOperation(QSharedPointer<NodeBinOP> bop) {
    if (bop->m_op.m_type == TokenType::PLUS)
        return "add";
    if (bop->m_op.m_type == TokenType::MINUS)
        return "sub";
    if (bop->m_op.m_type == TokenType::BITOR)
        return "or";
    if (bop->m_op.m_type == TokenType::BITAND)
        return "and";
    if (bop->m_op.m_type == TokenType::XOR)
        return "xor";
    if (bop->m_op.m_type == TokenType::DIV)
        return "idiv";
    if (bop->m_op.m_type == TokenType::MUL)
        return "imul";
    return " UNKNOWN BINARY OPERATION";
}

QString ASTdispatcherX86::getEndType(Assembler *as, QSharedPointer<Node> v)
{
    return "";
}




void ASTdispatcherX86::AssignString(QSharedPointer<NodeAssign> node, bool isPointer) {

    QSharedPointer<NodeString> right = qSharedPointerDynamicCast<NodeString>(node->m_right);
    QSharedPointer<NodeVar> left = qSharedPointerDynamicCast<NodeVar>(node->m_left);
    //    QString lbl = as->NewLabel("stringassign");
    QString str = as->NewLabel("stringassignstr");
    QString lblCpy=as->NewLabel("stringassigncpy");

    //    as->Asm("jmp " + lbl);
    QString strAssign = str + "\t db \"" + right->m_op.m_value + "\",0";
    as->m_tempVars<<strAssign;
    //as->Label(str + "\t.dc \"" + right->m_op.m_value + "\",0");
  //  as->Label(lbl);

//    qDebug() << "IS POINTER " << isPointer;
    if (isPointer) {
  //      qDebug() << "HERE";


        as->Asm("mov si, "+str+"");
        as->Asm("mov ["+left->getValue(as)+"+2], ds");
        as->Asm("mov ["+left->getValue(as)+"], si");

/*        as->Asm("lda #<"+str);
        as->Asm("sta "+getValue(left));
        as->Asm("lda #>"+str);
        as->Asm("sta "+getValue(left)+"+1");*/
    }
    else {
//        ErrorHandler::e.Error("String copying not yet implemented, can only be assigned as pointers.", node->m_op.m_lineNumber);
        as->Comment("String copy!");
//        as->
        if (left->isPointer(as))
            as->Asm("les di,["+left->getValue(as)+"]");
        else
            as->Asm("mov di,"+left->getValue(as)+"");
        as->Term();

        as->Asm("push ds");
        as->Asm("pop es");
        as->Asm("mov si,"+str);
        as->Asm("mov cx, "+Util::numToHex(right->m_op.m_value.count()+2));
        as->Asm("rep movsb");
//        as->Asm("pop ds");

/*        as->Asm("ldx #0");
        as->Label(lblCpy);
        as->Asm("lda " + str+",x");
        as->Asm("sta "+getValue(left) +",x");
        as->Asm("inx");
        as->Asm("cmp #0 ;keep");  // ask post optimiser to not remove this
        as->Asm("bne " + lblCpy);*/
    }
  //  as->PopLabel("stringassign");
    as->PopLabel("stringassignstr");
    as->PopLabel("stringassigncpy");

}




void ASTdispatcherX86::AssignVariable(QSharedPointer<NodeAssign> node)
{

    if (node->m_left->isWord(as)) {
        node->m_right->setForceType(TokenType::INTEGER);
    }

//    as->Comment("CUR TERM : " +as->m_term);
    as->ClearTerm();

    QSharedPointer<NodeVar> var = qSharedPointerDynamicCast<NodeVar>(node->m_left);


    QSharedPointer<Symbol> s = as->m_symTab->Lookup(getValue(var), node->m_op.m_lineNumber, var->isAddress());

    if (!var->isPurePointer(as) && node->m_right->isPurePointer(as)) {
        ErrorHandler::e.Error("Cannot assign pointer to variable "+var->getValue(as),var->m_op.m_lineNumber);
    }
    if (!var->isPurePointer(as) && node->m_right->isPurePointer(as)) {
        ErrorHandler::e.Error("Cannot assign pointer to variable "+var->getValue(as),var->m_op.m_lineNumber);
    }

    QString vname = getValue(var);
//    as->Comm nt("IS REGISTER : "+Util::numToHex(v->m_isRegister) + " "+vname);
    if (var->m_isRegister) {
        vname = vname.toLower();
        if (!node->m_right->isPure())
            ErrorHandler::e.Error("When assigning registers, RHS needs to be pure numeric or variable",node->m_op.m_lineNumber);

        QString reg = vname.remove(0,1);
//        as->Comment("Assigning register : " + vname);

        as->Asm("mov "+reg+", "+getX86Value(as,node->m_right));
        return;
        //}
    }

    if (qSharedPointerDynamicCast<NodeString>(node->m_right)) {
        AssignString(node,node->m_left->isPointer(as));
        return;
    }


    if (var->m_writeType==TokenType::INTEGER) {
        node->m_right->setForceType(TokenType::INTEGER);
    }
    if (var->m_writeType==TokenType::LONG)
        node->m_right->setForceType(TokenType::LONG);


    if (var->isPointer(as) && !var->isArrayIndex()) {
        node->m_right->VerifyReferences(as);
        if (!node->m_right->isReference())
            if (!node->m_right->isPointer(as))
                if (node->m_right->isWord(as) || node->m_right->isByte(as)) {
                ErrorHandler::e.Error("Trying to assign a non-pointer / non-reference / non-long to pointer '"+var->getValue(as)+"'",var->m_op.m_lineNumber);
                }

        as->Comment("Assigning pointer");

        QSharedPointer<NodeBinOP> bop =  qSharedPointerDynamicCast<NodeBinOP>(node->m_right);
//        node->m_right->setForceType(TokenType::POINTER);
        if (bop!=nullptr && (bop->m_op.m_type==TokenType::PLUS || bop->m_op.m_type==TokenType::MINUS || bop->m_op.m_type==TokenType::BITOR || bop->m_op.m_type==TokenType::BITAND || bop->m_op.m_type==TokenType::XOR )) {
            if (bop->m_left->getValue(as)==var->getValue(as)) {

                as->Comment("'p := p + v' optimization");
                as->ClearTerm();
                as->BinOP(bop->m_op.m_type);
                QString bopCmd = as->m_term;
                as->ClearTerm();
                if (bop->m_right->isPureNumeric()) {
                    as->Asm(bopCmd + " ["+var->getValue(as)+"], word "+bop->m_right->getValue(as));
                    return;
                }
                bop->m_right->Accept(this);
                as->Term();
                as->Asm(bopCmd + " ["+var->getValue(as)+"], ax");


                return;
            }
        }




        if (node->m_right->isPureVariable()) {
            if (node->m_right->isPointer(as)) {
                as->Asm("les di, ["+node->m_right->getValue(as)+"]");
                as->Asm("mov ["+var->getValue(as)+"+2], es");
                as->Asm("mov ["+var->getValue(as)+"], di");
            }
            else {
//                as->Asm("lea si, "+node->m_right->getValue(as));
//                as->Asm("cld");
                as->Asm("lea si, ["+node->m_right->getValue(as)+"]");
                //as->Asm("mov si, "+node->m_right->getValue(as));
                as->Asm("mov ["+var->getValue(as)+"+2], ds");
                as->Asm("mov ["+var->getValue(as)+"], si");
            }
            return;
        }
        else{
            as->Comment("Setting PURE POINTER "+QString::number(node->isPointer(as)));
//            m_isPurePointer = true;
 //           if (node->m_left->isPointer(as))
   //            node->m_right->setForceType(TokenType::POINTER);
            node->m_right->Accept(this);
  //          m_isPurePointer = false;
            as->Comment("Setting PURE POINTER ends");

            as->Asm("mov ["+var->getValue(as)+"+2], es");
            as->Asm("mov ["+var->getValue(as)+"], di");
        }
        return;
    }

    // Set pointer value
    if (var->isPointer(as) && var->isArrayIndex()) {

        // TO DO: Optimize special cases

        as->ClearTerm();
        as->Comment("Assigning pointer with index, type:" + TokenType::getType(var->m_writeType));
        if (var->isWord(as))
            node->m_right->setForceType(TokenType::INTEGER);
        node->m_right->Accept(this);

        as->Term();
        as->Asm("les di, ["+var->getValue(as)+"]");
        if (var->m_expr->isPureNumeric()) {

            as->Asm("mov [es:di+"+var->m_expr->getValue(as)+"*"+getIndexScaleVal(as,var)+"],"+getAx("a",var));
            return;

        }
        if (var->m_expr->isPureVariable() && var->m_expr->isWord(as)) {
            as->Asm("add di,["+var->m_expr->getValue(as)+"]");
            if (var->isWord(as))
                as->Asm("add di,["+var->m_expr->getValue(as)+"]");

            as->Asm("mov [es:di],"+getAx("a",var));

            return;

        }

        as->Asm("push ax");
        var->m_expr->setForceType(TokenType::INTEGER);
        var->m_expr->Accept(this);
        as->Term();
        if (var->isWord(as))
            as->Asm("shl ax,1");
        as->Asm("mov bx,ax");

        as->Asm("pop ax");

        as->Asm("mov [es:di+bx],"+getAx("a",var));
        return;

    }



    if (var->isArrayIndex()) {
        // Is an array
        as->Asm(";Is array index");
        if (var->getArrayType(as)==TokenType::POINTER) {
            as->Comment("Assign value to pointer array");
            node->m_right->Accept(this);
            var->m_expr->Accept(this);
            as->Asm("shl ax,2 ; pointer lookup");
            as->Asm("mov bx,ax");
            as->Asm("lea si,["+var->getValue(as)+"]");
            as->Asm("mov [ds:si+bx],di ; store in pointer array");
            as->Asm("mov [ds:si+bx+2],es");
/*
            as->Asm("mov bx,ax");
            as->Asm("shl bx,2");

            as->Asm("lea si,"+node->getValue(as)+ "");
            as->Asm("mov di,[ds:si+bx]");
            as->Asm("mov ax,[ds:si+bx+2]");
            as->Asm("mov es,ax");
  */

            return;
        }
        as->Comment("Assign value to regular array");
/*        node->m_right->Accept(this);
        as->Asm("push ax");
        var->m_expr->setForceType(TokenType::INTEGER);
        var->m_expr->Accept(this);
        as->Asm("mov di,ax");
        if (var->isWord(as))
            as->Asm("shl di,1");
        as->Asm("pop ax");
        as->Asm("mov ["+var->getValue(as) + "+di], "+getAx(node->m_left));
*/

        node->m_right->Accept(this);
        if (var->m_expr->isPure()) {
            var->m_expr->setForceType(TokenType::INTEGER);
            as->Asm("mov di,"+getX86Value(as,var->m_expr));
            if (var->isWord(as))
                as->Asm("shl di,1");
        }
        else {
            as->Asm("push ax");
            var->m_expr->setForceType(TokenType::INTEGER);
            var->m_expr->Accept(this);
            as->Asm("mov di,ax");
            if (var->isWord(as))
                as->Asm("shl di,1");
            as->Asm("pop ax");
        }
        as->Asm("mov ["+var->getValue(as) + "+di], "+getAx(node->m_left));


        return;
    }

//    if (var->getValue())
    // Simple a:=b;
    QString type =getWordByteType(as,var);

    if (node->m_right->isPureNumeric()) {
        as->Asm("mov ["+var->getValue(as)+ "], "+type+ " "+node->m_right->getValue(as));
        return;
    }
    // Check for a:=a+2;
    QSharedPointer<NodeBinOP> bop =  qSharedPointerDynamicCast<NodeBinOP>(node->m_right);
   // as->Comment("Testing for : a:=a+ expr " + QString::number(bop!=nullptr));
   // if (bop!=nullptr)
     //  as->Comment(TokenType::getType(bop->getType(as)));
    if (bop!=nullptr && (bop->m_op.m_type==TokenType::PLUS || bop->m_op.m_type==TokenType::MINUS || bop->m_op.m_type==TokenType::BITOR || bop->m_op.m_type==TokenType::BITAND || bop->m_op.m_type==TokenType::XOR )) {
  //      as->Comment("PREBOP searching for "+var->getValue(as));
        if (bop->ContainsVariable(as,var->getValue(as))) {
            // We are sure that a:=a ....
            // first, check if a:=a + number
//            as->Comment("In BOP");
            if (bop->m_right->isPureNumeric()) {
                as->Comment("'a:=a + const'  optimization ");
                as->Asm(getBinaryOperation(bop) + " ["+var->getValue(as)+"], "+type + " "+bop->m_right->getValue(as));
                return;
            }
            as->Comment("'a:=a + expression'  optimization ");
            bop->m_right->Accept(this);
            as->Asm(getBinaryOperation(bop) + " ["+var->getValue(as)+"], "+getAx(var));
            return;
        }
        // Check for a:=a+

    }
/*    if (node->m_right->isPureVariable()) {
        as->Asm("mov ["+var->getValue(as)+ "],   " +getX86Value(as,node->m_right));
        return;
    }
*/
    as->ClearTerm();
    node->m_right->Accept(this);
    as->Term();
    as->Asm("mov ["+qSharedPointerDynamicCast<NodeVar>(node->m_left)->getValue(as) + "], "+getAx(node->m_left));
    return;
}

void ASTdispatcherX86::DeclarePointer(QSharedPointer<NodeVarDecl> node)
{
    QSharedPointer<NodeVar> v = qSharedPointerDynamicCast<NodeVar>(node->m_varNode);
    QSharedPointer<NodeVarType> t = qSharedPointerDynamicCast<NodeVarType>(node->m_typeNode);

    if (Syntax::s.m_currentSystem->m_system == AbstractSystem::GAMEBOY || Syntax::s.m_currentSystem->m_system == AbstractSystem::COLECO)
        as->Write(v->getValue(as)+ ": ds  2" ,0);
    else
        if (Syntax::s.m_currentSystem->m_system == AbstractSystem::X86)
            as->Write(v->getValue(as)+ ": dw  0,0" ,0);
        else
            as->Write(v->getValue(as)+ ": dw  0",0);

    as->m_symTab->Lookup(v->getValue(as), node->m_op.m_lineNumber)->m_arrayType=t->m_arrayVarType.m_type;

}

QString ASTdispatcherX86::getEndType(Assembler *as, QSharedPointer<Node> v1, QSharedPointer<Node> v2)
{
    return "";
}

/*void ASTdispatcherX86::IncBin(Assembler *as, QSharedPointer<NodeVarDecl> node) {
    QSharedPointer<NodeVar> v = qSharedPointerDynamicCast<NodeVar>(node->m_varNode);
    QSharedPointer<NodeVarType> t = qSharedPointerDynamicCast<NodeVarType>(node->m_typeNode);
    QString filename = as->m_projectDir + "/" + t->m_filename;
    if (!QFile::exists(filename))
        ErrorHandler::e.Error("Could not locate binary file for inclusion :" +filename);

    int size=0;
    QFile f(filename);
    if (f.open(QIODevice::ReadOnly)){
        size = f.size();  //when file does open.
        f.close();
    }


    if (t->m_position=="") {

        as->Label(v->getValue(as));
        as->Asm("incbin \"" + filename + "\"");
    }
    else {
        //            qDebug() << "bin: "<<v->getValue(as) << " at " << t->m_position;
        //        Appendix app(t->m_position);

        QSharedPointer<Symbol> typeSymbol = as->m_symTab->Lookup(v->getValue(as), node->m_op.m_lineNumber);
        typeSymbol->m_org = Util::C64StringToInt(t->m_position);
        typeSymbol->m_size = size;
        //            qDebug() << "POS: " << typeSymbol->m_org;
        //app.Append("org " +t->m_position,1);

        as->Label(v->getValue(as));
        as->Asm("incbin \"" + filename + "\"");
        //      as->blocks.append(new MemoryBlock(start,start+size, MemoryBlock::DATA,t->m_filename));

    }
}

*/
void ASTdispatcherX86::BuildSimple(QSharedPointer<Node> node,  QString lblSuccess, QString lblFailed, bool page)
{

    as->Comment("Binary clause Simplified: " + node->m_op.getType());
    //    as->Asm("pha"); // Push that baby

    BuildToCmp(node);
    QString jg ="jg ";
    QString jl ="jl ";
    QString jge ="jge ";
    QString jle ="jle ";
    if (!(node->m_left->isSigned(as) || node->m_right->isSigned(as))) {
        jg = "ja ";
        jl = "jb ";
        jge = "jae ";
        jle = "jbe ";
    }
    if (node->m_op.m_type==TokenType::EQUALS)
        as->Asm("jne " + lblFailed);
    if (node->m_op.m_type==TokenType::NOTEQUALS)
        as->Asm("je " + lblFailed);
    if (node->m_op.m_type==TokenType::LESS)
        as->Asm(jge  + lblFailed);
    if (node->m_op.m_type==TokenType::GREATER)
        as->Asm(jle  + lblFailed);

    if (node->m_op.m_type==TokenType::LESSEQUAL)
        as->Asm(jg  + lblFailed);
    if (node->m_op.m_type==TokenType::GREATEREQUAL)
        as->Asm(jl  + lblFailed);



}

void ASTdispatcherX86::BuildToCmp(QSharedPointer<Node> node)
{
    if (node->m_left->getValue(as)!="") {
        if (node->m_right->isPureNumeric())
        {
            as->Comment("Compare with pure num / var optimization");
            //            TransformVariable(as,"cmp",node->m_left->getValue(as),node->m_right->getValue(as),node->m_left);
//            TransformVariable(as,"cmp",node->m_left->getValue(as),node->m_right->getValue(as),node->m_left);
            if (node->m_left->isPure()) {
                as->Asm("cmp ["+node->m_left->getValue(as)+"],"+getWordByteType(as,node->m_left)+" " + node->m_right->getValue(as));
                return;
            }

            LoadVariable(node->m_left);
            as->Asm("cmp "+getAx(node->m_left)+"," + node->m_right->getValue(as));

            return;
        } else
        {
            as->Comment("Compare two vars optimization");

            if (node->m_right->isPureVariable()) {
                //QString wtf = as->m_regAcc.Get();
                LoadVariable(node->m_right);
                //TransformVariable(as,"move",wtf,qSharedPointerDynamicCast<NodeVar>node->m_left);
                //TransformVariable(as,"cmp",wtf,as->m_varStack.pop());
                as->Asm("cmp  ["+node->m_left->getValue(as) +"]," + getAx(node->m_right));

                return;
            }
            node->m_right->Accept(this);
            as->Term();

            as->Asm("cmp  "+node->m_left->getValue(as) +"," + getAx(node->m_right));

//            TransformVariable(as,"cmp",qSharedPointerDynamicCast<NodeVar>node->m_left,as->m_varStack.pop());
            return;
        }
    }
    QString ax = getAx(node->m_left);
    QString bx = "b"+ QString(ax[1]);
    node->m_left->Accept(this);
    as->Term();
    if (node->m_right->isPure()) {
        as->Asm("cmp  "+ax+", " + getX86Value(as,node->m_right));
        return;

    }
    as->Comment("Evaluate full expression");
    as->Asm("push ax");
    node->m_right->Accept(this);
    as->Term();
    as->Asm("pop bx");

    as->Asm("cmp "+ax+","+bx);

//    TransformVariable(as,"cmp",qSharedPointerDynamicCast<NodeVar>node->m_left, as->m_varStack.pop());

    // Perform a full compare : create a temp variable
    //        QString tmpVar = as->m_regAcc.Get();//as->StoreInTempVar("binary_clause_temp");
    //        node->m_right->Accept(this);
    //      as->Asm("cmp " + tmpVar);
    //      as->PopTempVar();


}

void ASTdispatcherX86::CompareAndJumpIfNotEqualAndIncrementCounter(QSharedPointer<Node> nodeA, QSharedPointer<Node> nodeB, QSharedPointer<Node> step, QString lblJump, bool isOffPage, bool isInclusive)
{
    QString var = nodeA->m_left->getValue(as);
    if (step!=nullptr) {
        step->Accept(this);
        as->Asm("mov cx,ax");
    }
/*
    PushX();

    QString ax = getAx(nodeA->m_left);
    PopX();
    as->Asm(m_mov+ax+",["+var+"]");
    */
    if (step==nullptr)
        as->Asm("add ["+var+"],"+getWordByteType(as,nodeA->m_left)+" 1");
    else
        as->Asm("add ["+var+"],cx");

//    as->Asm(m_mov+"["+var+"],"+ax);
    LoadVariable(nodeB);
    as->Asm(m_cmp+getAx(nodeB)+","+getWordByteType(as,nodeA->m_left)+" ["+var+"]");
    as->Asm(m_jne+lblJump);

}

void ASTdispatcherX86::CompareAndJumpIfNotEqual(QSharedPointer<Node> nodeA, QSharedPointer<Node> nodeB, QString lblJump, bool isOffPage)
{
    if (nodeA->isWord(as)) nodeB->setForceType(TokenType::INTEGER);
    LoadVariable(nodeA);
    QString ax = getAx(nodeA);
    PushX();
    LoadVariable(nodeB);
    QString bx = getAx(nodeB);
    PopX();
    as->Asm(m_cmp+ax+","+bx);
    as->Asm(m_jne+lblJump);

}


