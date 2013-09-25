# The stuff

## has_component_views.js

The full explanation for this is at https://github.com/weeksie/has_component_views

It's totally obsoleted by Marionette, but was a nice thing to do at the time.

## has_metadata.rb

Sometimes it's groovy to put a serialized JSON hash on a record and
name it `meta`. Great. But when you're accessing the values in that
meta data it's even more groovy to define the acceptable values and
access them in a sane manner. The way it worked was like this:

```ruby
class MyFnord
  include HasMetadata

  meta_attr :skidoo
  meta_attr :immanentize_the_eschaton, default: 23
end

fnord = MyFnord.new
fnord.immanentize_the_eschaton # == 23
fnord.skidoo = "FNORD"
fnord.skidoo # == "FNORD"
fnord.meta # { "skidoo": "FNORD", "immanentize_the_eschaton": 23 }

```

## json_model.rb

Okay, so you have a ton of REST API model objects. And you have a ton
of whitelisted and blacklisted and funktified attributes that you want
to add to the JSON representation of that object. Cool at first. Sucky
after months of building out a monster system. This puppy lets you
declare how your JSON is built without constantly overriding your
`as_json` method. It works with inheritance, mixins, and all that jazz
too!

The mixin checks all of the args to `json_methods` to figure if
they're scalar values, lambdas, method_names, and so on.

```
class ImTotallyNotUsingAClassFromPlyfeForThisIPromise
  include JsonModel
  json_except :active_at, :reconciling_at, :created_at, :updated_at, :max_tweet_id, :presenters

  json_methods :type,
               :campaign_slug,
               :friendly_description,
               is_compound:          :compound?,
               success:              :challenge_succeeded,
               prepared:             :prepared_with_user?,
               can_complete_inline:  :can_complete_inline?,
               number_of_stages:     lambda { |c| c.stages.count },
               presenter_names:      :presenters,
               group_id:             :challenge_group_id,
               scoring_strategy:     lambda { |c| c.scoring_strategy.to_s.demodulize.underscore }
end
```

## PathPoint

### PathFind.lua

This is the interface to the C A* pathfinding code that the PathPoint
wayfinding software used. I <3 Lua. Great language. Simple, clean, and
easily the best C FFI I have ever worked with. There's lots of stuff
in there. Path smoothing, Bresnan's line of sight algo, etc. . . .

### pathfind.c, pathfind.h

Modified A* algo. Bresnan's line of sight as well. Dig in. Also, lots
of tasty Lua integration.
