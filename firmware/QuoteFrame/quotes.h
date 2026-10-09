// quotes.h — which passages of Magnifica Humanitas the frame shows.
//
// Each line is { paragraph, first sentence, number of sentences }. Paragraphs
// are the document's own numbers; sentences count from 0 and are split the
// way splitSentences() in quote_source.h does it (footnote markers removed).
//
// The words themselves are read from vatican.va when the frame wakes, so
// nothing from the text is stored here. To see exactly what each line gives,
// or to find new ones, run tests/preview_quotes (see the README).
//
// Listed in reading order. The frame doesn't show them in this order: with
// SPREAD_QUOTES on (config.h) it steps through the list with a stride, so
// consecutive days come from different parts of the letter. Every entry still
// comes up once per cycle. Add, remove or reorder freely.

#pragma once
#include "quote_source.h"

static const QuoteRef QUOTES[] = {
  // ---- Introduction ----
  {  1, 0, 1 },   // the pivotal choice: Babel or the shared city
  {  6, 4, 2 },   // two questions about where we are heading
  {  7, 7, 2 },   // a city built on pride: dispersion, not unity
  {  9, 2, 1 },   // technology can heal and connect, or divide and exclude
  {  9, 3, 2 },   // technology is neither a cure-all nor evil, and never neutral
  { 10, 1, 1 },   // dehumanization as an old temptation in technical dress
  { 11, 2, 1 },   // Augustine: the restless heart
  { 12, 4, 2 },   // fulfilment through growth, not by abolishing weakness
  { 13, 1, 1 },   // no one carries it alone; no one too weak to help
  { 13, 4, 1 },   // tensions can become creative forces
  { 14, 1, 2 },   // words that shed light; neither naive nor fearful
  { 15, 2, 1 },   // the duty to remain profoundly human
  { 15, 3, 1 },   // a grandeur no machine can replace
  { 15, 4, 1 },   // where true progress comes from
  { 16, 2, 1 },   // like Nehemiah: pray, plan, persevere
  { 16, 4, 2 },   // builders of communion, not architects of Babel

  // ---- Chapter one: a dynamic approach faithful to the Gospel ----
  { 25, 0, 1 },   // truth as a gift to share, not a possession
  { 25, 4, 1 },   // starting good processes rather than holding ground

  // ---- Chapter two: foundations and principles ----
  { 50, 2, 1 },   // dignity is a gift, not earned by ability or wealth
  { 51, 5, 1 },   // a person's worth is not what they produce
  { 52, 7, 1 },   // nothing can diminish the value of a human life
  { 53, 0, 1 },   // dignity is neither acquired nor earned
  { 55, 3, 1 },   // the right to life comes first
  { 58, 0, 2 },   // it is persons who matter
  { 62, 0, 1 },   // what makes a people
  { 67, 0, 1 },   // algorithms, platforms and data as shared goods
  { 73, 3, 2 },   // fraternity made real; no one is saved alone
  { 74, 2, 1 },   // not just neighbours: entrusted to each other
  { 76, 3, 1 },   // the digital ecosystem, preserved or exploited
  { 81, 0, 2 },   // how a society treats migrants
  { 83, 2, 1 },   // development that puts people at the centre
  { 85, 1, 1 },   // AI is not neutral
  { 88, 2, 1 },   // the Eucharist teaches us to share
  { 89, 2, 2 },   // all power and authority as service

  // ---- Chapter three: technology and dominance ----
  { 90, 4, 1 },   // what are we building?
  { 92, 1, 2 },   // when technology becomes the measure of everything
  { 93, 3, 1 },   // more power is not necessarily better
  { 94, 2, 1 },   // having more without being more
  { 99, 5, 1 },   // what AI does not have: experience, body, joy, love
  {100, 8, 1 },   // simulated words build only the appearance of relationship
  {104, 1, 1 },   // every tool embodies choices in what it measures and ignores
  {106, 0, 1 },   // prudence and a slower pace are not opposition to progress
  {107, 2, 1 },   // a more moral AI is not enough if a few decide the morality
  {108, 5, 1 },   // data is the product of many contributors
  {110, 3, 2 },   // to disarm: technology without dominance
  {111, 2, 1 },   // every design choice reflects a vision of humanity
  {112, 4, 1 },   // persons, not projects to be optimized
  {114, 0, 1 },   // a civilization is measured by its care
  {114, 2, 2 },   // reading to a child, keeping company with the elderly
  {118, 2, 1 },   // humanity flourishes through its limits, not despite them
  {120, 1, 1 },   // eliminating suffering would extinguish love too
  {125, 3, 2 },   // the martyrs of everyday life
  {126, 1, 1 },   // humanity must never be replaced or surpassed
  {128, 4, 1 },   // an error: a flaw for an algorithm, a turning point for a person
  {128, 5, 1 },   // a person's future is not calculable
  {129, 2, 1 },   // the real choice is not enthusiasm or fear
  {130, 7, 1 },   // Babel or Jerusalem begins within each of us

  // ---- Chapter four: truth, work, freedom ----
  {132, 2, 1 },   // disinformation finds an amplifier in AI
  {134, 3, 1 },   // indifference to truth and the slide to totalitarianism
  {137, 0, 1 },   // neither demonize nor idolize our tools
  {140, 2, 1 },   // teaching when AI ought not to be used
  {140, 3, 1 },   // easy answers can extinguish the desire to ask
  {140, 5, 1 },   // protecting the young from the promise of the perfect machine
  {143, 0, 1 },   // what school is for
  {147, 2, 1 },   // schools offer what the digital world cannot
  {149, 0, 1 },   // work expresses the dignity of our lives
  {152, 2, 1 },   // profit cannot justify sacrificing jobs
  {161, 5, 1 },   // new technology won't benefit everyone automatically
  {170, 2, 1 },   // business models that thrive on human weakness
  {173, 1, 2 },   // nothing in AI is magical: the people behind it
  {173, 11, 1 },  // emancipation that produces subordination
  {178, 0, 2 },   // colonialism that appropriates data

  // ---- Chapter five: the culture of power and the civilization of love ----
  {182, 2, 1 },   // peace as a prerequisite for the common good
  {192, 6, 1 },   // dialogue, diplomacy and forgiveness
  {195, 2, 1 },   // easier to start a war than to stop it
  {198, 3, 1 },   // no algorithm can make war morally acceptable
  {199, 8, 1 },   // attacks without seeing a face
  {205, 5, 1 },   // peace as the fruit of justice and charity
  {210, 6, 1 },   // goodness grows silently from the earth
  {211, 1, 1 },   // even in the darkest nights
  {212, 0, 2 },   // resignation disguised as realism
  {213, 0, 1 },   // Tolkien: uprooting the evil in the fields we know
  {213, 1, 1 },   // small, steadfast acts of fidelity
  {214, 1, 1 },   // disarm words to disarm the world
  {214, 6, 1 },   // each time we speak the truth
  {215, 7, 1 },   // never grow weary of seeking justice
  {216, 0, 1 },   // sometimes, to remain human, we must take a stand
  {219, 1, 1 },   // nothing is lost with peace
  {220, 2, 1 },   // encounter makes war harder to imagine
  {222, 3, 1 },   // the peacemakers make history
  {222, 4, 1 },   // neighbours, not enemies

  // ---- Conclusion ----
  {229, 0, 1 },   // let each builder choose with care
  {232, 10, 1 },  // divine love renews history from within
  {233, 3, 1 },   // no computational system can create a heart
  {233, 4, 1 },   // a human face remains the centre of history
  {233, 7, 1 },   // nothing authentically human will be lost
  {238, 0, 1 },   // invest in education, beginning with ourselves
  {238, 5, 1 },   // technology does not follow a predetermined path
  {239, 1, 2 },   // the heart's need for genuine closeness
  {241, 3, 1 },   // listening and courage, prayer and responsibility
  {245, 0, 1 },   // weavers of hope
  {245, 1, 1 },   // even the era of AI can become a time of grace
};
static const int QUOTE_COUNT = sizeof(QUOTES) / sizeof(QUOTES[0]);
