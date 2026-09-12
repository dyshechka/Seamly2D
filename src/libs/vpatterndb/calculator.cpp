/***************************************************************************
 *                                                                         *
 *   Copyright (C) 2017  Seamly, LLC                                       *
 *                                                                         *
 *   https://github.com/fashionfreedom/seamly2d                             *
 *                                                                         *
 ***************************************************************************
 **
 **  Seamly2D is free software: you can redistribute it and/or modify
 **  it under the terms of the GNU General Public License as published by
 **  the Free Software Foundation, either version 3 of the License, or
 **  (at your option) any later version.
 **
 **  Seamly2D is distributed in the hope that it will be useful,
 **  but WITHOUT ANY WARRANTY; without even the implied warranty of
 **  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 **  GNU General Public License for more details.
 **
 **  You should have received a copy of the GNU General Public License
 **  along with Seamly2D.  If not, see <http://www.gnu.org/licenses/>.
 **
 **************************************************************************

 ************************************************************************
 **
 **  @file   calculator.cpp
 **  @author Roman Telezhynskyi <dismine(at)gmail.com>
 **  @date   November 15, 2013
 **
 **  @brief
 **  @copyright
 **  This source code is part of the Valentine project, a pattern making
 **  program, whose allow create and modeling patterns of clothing.
 **  Copyright (C) 2013-2015 Seamly2D project
 **  <https://github.com/fashionfreedom/seamly2d> All Rights Reserved.
 **
 **  Seamly2D is free software: you can redistribute it and/or modify
 **  it under the terms of the GNU General Public License as published by
 **  the Free Software Foundation, either version 3 of the License, or
 **  (at your option) any later version.
 **
 **  Seamly2D is distributed in the hope that it will be useful,
 **  but WITHOUT ANY WARRANTY; without even the implied warranty of
 **  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 **  GNU General Public License for more details.
 **
 **  You should have received a copy of the GNU General Public License
 **  along with Seamly2D.  If not, see <http://www.gnu.org/licenses/>.
 **
 *************************************************************************/

#include "calculator.h"

#include <QObject>
#include <QString>
#include <QStringList>

#include "../vmisc/def.h"
#include "../qmuparser/qmuparsererror.h"
#include "variables/vinternalvariable.h"
#include <QSharedPointer>
//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief Calculator class wraper for QMuParser. Make easy initialization math parser.
 *
 * This constructor hide initialization variables, operators, character sets.
 * Use this constuctor for evaluation formula. All formulas must be converted to internal look.
 * Example:
 *
 * const QString formula = qApp->FormulaFromUser(edit->text());
 * Calculator *cal = new Calculator(data, patternType);
 * const qreal result = cal->EvalFormula(data->PlainVariables(), formula);
 * delete cal;
 *
 */
Calculator::Calculator()
    :QmuFormulaBase()
{
    InitCharSets();
    setAllowSubexpressions(false);//Only one expression per time

    SetSepForEval();
}

//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief eval calculate formula.
 *
 * First we try eval expression without adding variables. If it fail, we take tokens from expression and add variables
 * to parser and try again.
 *
 * @param formula string of formula.
 * @return value of formula.
 */
qreal Calculator::EvalFormula(const QHash<QString, QSharedPointer<VInternalVariable>> *vars, const QString &formula)
{
    // Parser doesn't know any variable on this stage. So, we just use variable factory that for each unknown variable
    // set value to 0.
    SetVarFactory(AddVariable, this);
    SetSepForEval();//Reset separators options

    SetExpr(formula);

    qreal result = 0;
    result = Eval();

    QMap<int, QString> tokens = this->GetTokens();

    // Remove "-" from tokens list if exist. If don't do that unary minus operation will broken.
    RemoveAll(tokens, QStringLiteral("-"));

    for (int i = 0; i < builInFunctions.size(); ++i)
    {
        if (tokens.isEmpty())
        {
            break;
        }
        RemoveAll(tokens, builInFunctions.at(i));
    }

    if (tokens.isEmpty())
    {
        return result; // We have found only numbers in expression.
    }

    // Add variables to parser because we have deal with expression with variables.
    InitVariables(vars, tokens, formula);
    return Eval();
}

//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief Calculator::GetUsedVariables list the variable/measurement names referenced by a
 * formula. See the declaration in calculator.h for why this exists and what it reuses from
 * EvalFormula() above.
 */
QStringList Calculator::GetUsedVariables(const QString &formula)
{
    // Parser doesn't know any variable at this stage either -- same var factory trick as
    // EvalFormula() above, so an unknown name doesn't throw before we get a chance to list it.
    SetVarFactory(AddVariable, this);
    SetSepForEval();
    SetExpr(formula);
    Eval();

    QMap<int, QString> tokens = this->GetTokens();

    // Same cleanup as EvalFormula() above: strip the unary-minus token and built-in function
    // names, leaving only genuine variable/measurement names.
    RemoveAll(tokens, QStringLiteral("-"));
    for (int i = 0; i < builInFunctions.size(); ++i)
    {
        if (tokens.isEmpty())
        {
            break;
        }
        RemoveAll(tokens, builInFunctions.at(i));
    }

    return tokens.values();
}

//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief Calculator::InitVariables add variables to parser.
 *
 * For optimization purpose we try don't add variables that we don't need.
 *
 * @param vars list of variables.
 * @param tokens all tokens (measurements names, variables with lengths) that parser have found in expression.
 * @param formula expression, need for throwing better error message.
 */
void Calculator::InitVariables(const QHash<QString, QSharedPointer<VInternalVariable> > *vars,
                               const QMap<int, QString> &tokens, const QString &formula)
{
    // Only used for the generic "Unexpected token" error this function used to throw below --
    // no longer needed now that this has its own, more specific message. Left as a parameter
    // (rather than changed to remove it) since it documents what the tokens/positions below are
    // positions *into*, for anyone reading vs. calling this.
    Q_UNUSED(formula);

    QMap<int, QString>::const_iterator i = tokens.constBegin();
    while (i != tokens.constEnd())
    {
        bool found = false;
        if (vars->contains(i.value()))
        {
            DefineVar(i.value(), vars->value(i.value())->GetValue());
            found = true;
        }

        if (found == false && builInFunctions.contains(i.value()))
        {// We have found built-in function
            found = true;
        }

        if (found == false)
        {
            // By this point the formula has already survived a full parse (EvalFormula()'s
            // first Eval() pass, with a stub variable factory that accepts anything shaped
            // like a name) -- so this token is never raw syntax garbage; garbage would already
            // have thrown one of the parser's own "Unexpected ..." errors before this function
            // ever ran. What lands here is always a token that parses fine AS A NAME but isn't
            // one this formula actually has -- most often a mistyped, wrong-script (a Cyrillic
            // letter standing in for its Latin look-alike, or the reverse), or no-longer-
            // existing name. The generic parser wording ("Unexpected token") doesn't tell those
            // two situations apart, which is exactly what made a mismatched name so hard to
            // track down -- so this gets its own, more actionable message instead of reusing
            // qmu::ecUNASSIGNABLE_TOKEN's generic text (see qmuparsererror.cpp).
            throw qmu::QmuParserError(
                QObject::tr("Variable \"$TOK$\" not found (position $POS$). Check that the name is "
                            "typed correctly (including letter case and keyboard layout) and that "
                            "it wasn't deleted or renamed."),
                i.key(), i.value());
        }
        ++i;
    }
}
